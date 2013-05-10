/*------------------------------------------------------------------------*/
/*                 Copyright 2010, 2011 Sandia Corporation.               */
/*  Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive   */
/*  license for use of this work by or on behalf of the U.S. Government.  */
/*  Export of this program may require a license from the                 */
/*  United States Government.                                             */
/*------------------------------------------------------------------------*/

/**
 * @author H. Carter Edwards
 */

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <stk_util/environment/ReportHandler.hpp>

#include <stk_util/util/StaticAssert.hpp>

#include <stk_util/diag/Trace.hpp>
#include <stk_util/parallel/ParallelComm.hpp>
#include <stk_util/parallel/ParallelReduce.hpp>

#include <stk_mesh/base/Bucket.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/Comm.hpp>
#include <stk_mesh/base/FieldData.hpp>
#include <stk_mesh/base/FindRestriction.hpp>
#include <stk_mesh/baseImpl/EntityRepository.hpp>

#include <boost/foreach.hpp>

namespace stk {
namespace mesh {

namespace {

parallel::DistributedIndex::KeySpanVector
convert_entity_keys_to_spans( const MetaData & meta )
{
  // Make sure the distributed index can handle the EntityKey

  enum { OK = StaticAssert<
                SameType< uint64_t,
                          parallel::DistributedIndex::KeyType >::value >::OK };

  // Default constructed EntityKey has all bits set.

  const EntityKey invalid_key ;
  const EntityId  min_id = 1 ;
  const EntityId  max_id = invalid_key.id();

  const size_t rank_count = meta.entity_rank_count();

  parallel::DistributedIndex::KeySpanVector spans( rank_count );

  for ( size_t rank = 0 ; rank < rank_count ; ++rank ) {
    EntityKey key_min( rank , min_id );
    EntityKey key_max( rank , max_id );
    spans[rank].first  = key_min;
    spans[rank].second = key_max;
  }

  return spans ;
}

}

//----------------------------------------------------------------------

stk::mesh::FieldBase* try_to_find_coord_field(const stk::mesh::MetaData& meta)
{
  //attempt to initialize the coordinate-field pointer, trying a couple
  //of commonly-used names. It is expected that the client code will initialize
  //the coordinates field using BulkData::set_coordinate_field, but this is an
  //attempt to be helpful for existing client codes which aren't yet calling that.

  stk::mesh::FieldBase* coord_field = meta.get_field("mesh_model_coordinates");
  if (coord_field == NULL) {
    coord_field = meta.get_field("mesh_model_coordinates_0");
  }
  if (coord_field == NULL) {
    coord_field = meta.get_field("model_coordinates");
  }
  if (coord_field == NULL) {
    coord_field = meta.get_field("model_coordinates_0");
  }
  if (coord_field == NULL) {
    coord_field = meta.get_field("coordinates");
  }

  return coord_field;
}

//----------------------------------------------------------------------

#ifdef  STK_MESH_ALLOW_DEPRECATED_ENTITY_FNS
BulkData * BulkData::the_bulk_data_registry[MAX_NUM_BULKDATA] = {};
#endif

//----------------------------------------------------------------------

BulkData::BulkData( MetaData & mesh_meta_data ,
                    ParallelMachine parallel ,
                    unsigned bucket_max_size ,
                    bool use_memory_pool
#ifdef SIERRA_MIGRATION
                    , bool add_fmwk_data
#endif
                    , bool maintain_fast_indices
                    , ConnectivityMap* connectivity_map
                    )
  : m_entities_index( parallel, convert_entity_keys_to_spans(mesh_meta_data) ),
    m_entity_repo(*this, use_memory_pool),
    m_entity_comm_list(),
    m_ghosting(),
    m_deleted_entities(),
    m_deleted_entities_current_modification_cycle(),
    m_coord_field(NULL),
    m_mesh_meta_data( mesh_meta_data ),
    m_parallel_machine( parallel ),
    m_parallel_size( parallel_machine_size( parallel ) ),
    m_parallel_rank( parallel_machine_rank( parallel ) ),
    m_sync_count( 0 ),
    m_sync_state( MODIFIABLE ),
    m_meta_data_verified( false ),
    m_optimize_buckets(false),
    m_mesh_finalized(false),
#ifdef STK_MESH_ALLOW_DEPRECATED_ENTITY_FNS
    m_bulk_data_id(0),
#endif
#ifdef SIERRA_MIGRATION
    m_add_fmwk_data(add_fmwk_data),
    m_fmwk_bulk_ptr(NULL),
    m_check_invalid_rels(true),
#endif
    m_maintain_fast_indices(maintain_fast_indices),
    m_num_fields(-1), // meta data not necessarily committed yet
    m_mesh_indexes(),
    m_entity_keys(),
    m_entity_states(),
    m_entity_sync_counts(),
#ifdef SIERRA_MIGRATION
    m_fmwk_aux_relations(),
    m_fmwk_global_ids(),
    m_fmwk_local_ids(),
    m_fmwk_shared_attrs(),
    m_fmwk_connect_counts(),
#endif
    m_field_meta_data(),
    m_field_raw_data(mesh_meta_data.entity_rank_count()),
    m_bucket_repository(
        *this,
        bucket_max_size,
        mesh_meta_data.entity_rank_count(),
        m_entity_repo,
        connectivity_map != NULL ? *connectivity_map :
           (mesh_meta_data.spatial_dimension() == 2 ? ConnectivityMap::default_map_2d() : ConnectivityMap::default_map())
/*           (mesh_meta_data.spatial_dimension() == 2 ? ConnectivityMap::fixed_edges_map_2d() : ConnectivityMap::fixed_edges_map()) */
        )
{
  initialize_arrays();

  m_coord_field = try_to_find_coord_field(mesh_meta_data);

  create_ghosting( "shared" );
  create_ghosting( "shared_aura" );

#ifdef STK_MESH_ALLOW_DEPRECATED_ENTITY_FNS
  static int bulk_data_id = 0;
  m_bulk_data_id = bulk_data_id++;
  the_bulk_data_registry[m_bulk_data_id] = this;
#endif

  m_sync_state = SYNCHRONIZED ;
}

BulkData::~BulkData()
{
#ifdef SIERRA_MIGRATION
  for(size_t i=0; i<m_fmwk_aux_relations.size(); ++i) {
    delete m_fmwk_aux_relations[i];
  }
#endif

  while ( ! m_ghosting.empty() ) {
    delete m_ghosting.back();
    m_ghosting.pop_back();
  }
#ifdef STK_MESH_ALLOW_DEPRECATED_ENTITY_FNS
  the_bulk_data_registry[m_bulk_data_id] = NULL;
#endif
}

void BulkData::update_deleted_entities_container()
{
  //Question: should the m_deleted_entities container be sorted and uniqued?
  //I.e., should we guard against the same entity being deleted in consecutive modification cycles?

  while(!m_deleted_entities_current_modification_cycle.empty()) {
    size_t entity_offset = m_deleted_entities_current_modification_cycle.front();
    m_deleted_entities_current_modification_cycle.pop_front();
    m_deleted_entities.push_front(entity_offset);
  }
}

size_t BulkData::total_field_data_footprint(EntityRank rank) const
{
  const std::vector< FieldBase * > & field_set = mesh_meta_data().get_fields();

  size_t retval = 0;
  for ( int i = 0; i < m_num_fields; ++i) {
    const FieldBase  & field = * field_set[i];
    retval += total_field_data_footprint(field, rank);
  }

  return retval;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------

void BulkData::require_ok_to_modify() const
{
  ThrowRequireMsg( m_sync_state != SYNCHRONIZED,
                   "NOT in the ok-to-modify state" );
}

void BulkData::require_entity_owner( const Entity entity ,
                                     int owner ) const
{
  if (parallel_size() > 1 && bucket_ptr(entity) != NULL) {
    const bool error_not_owner = owner != parallel_owner_rank(entity) ;

    ThrowRequireMsg( !error_not_owner,
                     "Entity " << identifier(entity) << " owner is " <<
                     parallel_owner_rank(entity) << ", expected " << owner);
  }
}

void BulkData::require_good_rank_and_id(EntityRank ent_rank, EntityId ent_id) const
{
  const size_t rank_count = m_mesh_meta_data.entity_rank_count();
  const bool ok_id   = EntityKey::is_valid_id(ent_id);
  const bool ok_rank = ent_rank < rank_count && !(ent_rank == MetaData::FACE_RANK && mesh_meta_data().spatial_dimension() == 2);

  ThrowRequireMsg( ok_rank,
                   "Bad key rank: " << ent_rank << " for id " << ent_id );

  ThrowRequireMsg( ok_id, "Bad id : " << ent_id);
}

void BulkData::require_metadata_committed()
{
  if (!m_mesh_meta_data.is_commit()) {
    m_mesh_meta_data.commit();
  }
}

//----------------------------------------------------------------------

bool BulkData::modification_begin()
{
  Trace_("stk::mesh::BulkData::modification_begin");

  parallel_machine_barrier( m_parallel_machine );

  ThrowRequireMsg( m_mesh_finalized == false, "Unable to modifiy, BulkData has been finalized.");

  if (m_sync_count == 0) {
    m_mesh_meta_data.set_mesh_on_fields(this);
  }

  if ( m_sync_state == MODIFIABLE && m_mesh_finalized == false ) return false ;

  if ( ! m_meta_data_verified ) {
    require_metadata_committed();

    if (parallel_size() > 1) {
      verify_parallel_consistency( m_mesh_meta_data , m_parallel_machine );
    }

    m_meta_data_verified = true ;
  }
  else {
    ++m_sync_count ;

    //Set all entity states to 'Unchanged',
    for ( impl::EntityRepository::iterator
            i = m_entity_repo.begin() ; i != m_entity_repo.end() ; ++i )
    {
      m_entity_states[i->second.local_offset()] = Unchanged;
    }
  }

  // // It might be overkill to call this on every modification cycle.

  m_sync_state = MODIFIABLE ;

  return true ;
}

void BulkData::modified(Entity entity)
{
  TraceIfWatching("stk::mesh::BulkData::log_modified_and_propagate", LOG_ENTITY, entity_key(entity));

  // If already in modified state, return
  EntityState entity_state = this->state(entity);
  if (entity_state != Unchanged) {
    return;
  }

  // mark this entity as modified
  this->set_state(entity, Modified);

  // recurse on related entities w/ higher rank
  // outer loop iterates backwards as an optimization to reduce function call depth.
  EntityRank rank_of_original_entity = entity_rank(entity);
  for (EntityRank irank = m_mesh_meta_data.entity_rank_count() - 1;
        irank > rank_of_original_entity;
        --irank)
  {
    Entity const *rels_i = begin_entities(entity, irank);
    Entity const *rels_e = end_entities(entity, irank);
    for (; rels_i != rels_e; ++rels_i)
    {
      Entity other_entity = *rels_i;
      if ( this->state(other_entity) == Unchanged ) {
        this->modified(other_entity);
      }
    }
  }
}

size_t BulkData::count_relations(Entity entity) const
{
  const MeshIndex &mesh_idx = mesh_index(entity);

  const EntityRank end_rank = m_mesh_meta_data.entity_rank_count();
  size_t count = 0;
  for (EntityRank irank = stk::topology::BEGIN_RANK; irank < end_rank; ++irank)
  {
    count += mesh_idx.bucket->num_connectivity(mesh_idx.bucket_ordinal, irank);
  }
  return count;
}

bool BulkData::has_no_relations(Entity entity) const
{
  const MeshIndex &mesh_idx = mesh_index(entity);

  const EntityRank end_rank = m_mesh_meta_data.entity_rank_count();
  for (EntityRank irank = stk::topology::BEGIN_RANK; irank < end_rank; ++irank)
  {
    if (mesh_idx.bucket->num_connectivity(mesh_idx.bucket_ordinal, irank) > 0)
    {
      return false;
    }
  }
  return true;
}

unsigned BulkData::count_valid_connectivity(Entity entity, EntityRank rank) const
{
  m_check_invalid_rels = false;
  Entity const *rel_iter = begin_entities(entity, rank);
  Entity const *rel_end = end_entities(entity, rank);
  m_check_invalid_rels = true;

  unsigned count = 0;
  for (; rel_iter != rel_end; ++rel_iter)
  {
    if (rel_iter->is_local_offset_valid())
    {
      ++count;
    }
  }
  return count;
}

unsigned BulkData::count_valid_connectivity(Entity entity) const
{
  unsigned count = 0;
  const EntityRank end_rank = m_mesh_meta_data.entity_rank_count();
  for (EntityRank irank = stk::topology::BEGIN_RANK; irank < end_rank; ++irank)
  {
    count += count_valid_connectivity(entity, irank);
  }
  return count;
}

size_t BulkData::generate_next_local_offset()
{
  size_t new_local_offset = m_mesh_indexes.size();

  if (!m_deleted_entities.empty()) {
    new_local_offset = m_deleted_entities.front();
    m_deleted_entities.pop_front();
  }

  MeshIndex mesh_index = {NULL, 0};
  EntityKey invalid_key;

  if (new_local_offset == m_mesh_indexes.size()) {
    m_mesh_indexes.push_back(mesh_index);
    m_entity_keys.push_back(invalid_key);
    m_entity_states.push_back(Created);
    m_entity_sync_counts.push_back(0);
  
#ifdef SIERRA_MIGRATION
    if (m_add_fmwk_data) {
      m_fmwk_aux_relations.push_back(NULL);
      m_fmwk_global_ids.push_back(0);
      m_fmwk_local_ids.push_back(sierra::Fmwk::INVALID_LOCAL_ID);
      m_fmwk_shared_attrs.push_back(NULL);
      m_fmwk_connect_counts.push_back(0);
    }
#endif
  }
  else {
    //re-claiming space from a previously-deleted entity:

    m_mesh_indexes[new_local_offset] = mesh_index;
    m_entity_keys[new_local_offset] = invalid_key;
    m_entity_states[new_local_offset] = Created;
    m_entity_sync_counts[new_local_offset] = 0;
  
#ifdef SIERRA_MIGRATION
    if (m_add_fmwk_data) {
      //bulk-data allocated aux-relation vector, so delete it here.
      delete m_fmwk_aux_relations[new_local_offset];
      m_fmwk_aux_relations[new_local_offset] = NULL;
      m_fmwk_global_ids[new_local_offset] = 0;
      m_fmwk_local_ids[new_local_offset] = sierra::Fmwk::INVALID_LOCAL_ID;
      //don't delete shared-attr, it was allocated by fmwk.
      m_fmwk_shared_attrs[new_local_offset] = NULL;
      m_fmwk_connect_counts[new_local_offset] = 0;
    }
#endif
  }

  return new_local_offset;
}

void BulkData::initialize_arrays()
{
  ThrowRequireMsg((m_mesh_indexes.size() == 0) && (m_entity_keys.size() == 0)
                        && (m_entity_states.size() == 0) && (m_entity_sync_counts.size() == 0),
                   "BulkData::initialize_arrays() called by something other than constructor");

  MeshIndex mesh_index = {NULL, 0};
  m_mesh_indexes.push_back(mesh_index);

  EntityKey invalid_key;
  m_entity_keys.push_back(invalid_key);

  m_entity_states.push_back(Deleted);
  m_entity_sync_counts.push_back(0);

#ifdef SIERRA_MIGRATION
  if (m_add_fmwk_data) {
    m_fmwk_aux_relations.push_back(NULL);
    m_fmwk_global_ids.push_back(0);
    m_fmwk_local_ids.push_back(sierra::Fmwk::INVALID_LOCAL_ID);
    m_fmwk_shared_attrs.push_back(NULL);
    m_fmwk_connect_counts.push_back(0);
  }
#endif
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// The add_parts must be full ordered and consistent,
// i.e. no bad parts, all supersets included, and
// owner & used parts match the owner value.

//----------------------------------------------------------------------

Entity BulkData::declare_entity( EntityRank ent_rank , EntityId ent_id ,
                                 const PartVector & parts )
{
  m_check_invalid_rels = false;

  require_ok_to_modify();

  require_good_rank_and_id(ent_rank, ent_id);

  EntityKey key( ent_rank , ent_id );
  TraceIfWatching("stk::mesh::BulkData::declare_entity", LOG_ENTITY, key);
  DiagIfWatching(LOG_ENTITY, key, "declaring entity with parts " << parts);

  std::pair< Entity , bool > result = m_entity_repo.internal_create_entity( key );

  Entity declared_entity = result.first;

  if ( !result.second) {
    // An existing entity, the owner must match.
    require_entity_owner( declared_entity , m_parallel_rank );
    DiagIfWatching(LOG_ENTITY, key, "existing entity: " << declared_entity);
  }

  //------------------------------

  Part * const owns = & m_mesh_meta_data.locally_owned_part();

  PartVector rem ;
  PartVector add( parts );
  add.push_back( owns );

  change_entity_parts( declared_entity , add , rem );

  if ( result.second ) {
    this->set_parallel_owner_rank(declared_entity, m_parallel_rank);
    set_synchronized_count(declared_entity, m_sync_count);
    DiagIfWatching(LOG_ENTITY, key, "new entity: " << declared_entity);
  }

  m_check_invalid_rels = true;

  return declared_entity ;
}

Entity BulkData::declare_entity( EntityRank ent_rank , EntityId ent_id)
{
    Part& universal = mesh_meta_data().universal_part();
    return declare_entity(ent_rank, ent_id, universal);
}

void BulkData::change_entity_id( EntityId id, Entity entity)
{
  ThrowAssertMsg(parallel_size() == 1,
                 "change_entity_id only supported in serial");

  EntityRank e_rank = entity_rank(entity);

  require_ok_to_modify();
  require_good_rank_and_id(e_rank, id);

  EntityKey new_key(e_rank,id);
  EntityKey old_key = entity_key(entity);

  internal_change_entity_key(old_key, new_key, entity);
}

void BulkData::internal_change_entity_key( EntityKey old_key, EntityKey new_key, Entity entity)
{
  m_entity_repo.update_entity_key(new_key, old_key, entity);
  set_entity_key(entity, new_key);
}

//----------------------------------------------------------------------

bool BulkData::destroy_entity( Entity entity )
{
  TraceIfWatching("stk::mesh::BulkData::destroy_entity", LOG_ENTITY, entity_key(entity));
  DiagIfWatching(LOG_ENTITY, entity_key(entity), "entity state: " << entity);

  require_ok_to_modify();

  m_check_invalid_rels = false;

  if (!is_valid(entity)) {
    m_check_invalid_rels = true;
    return false;
  }

  const EntityRank end_rank = m_mesh_meta_data.entity_rank_count();
  for (EntityRank irank = entity_rank(entity) + 1; irank != end_rank; ++irank) {
    if (num_connectivity(entity, irank) > 0) {
      m_check_invalid_rels = true;
      return false;
    }
  }

  //------------------------------
  // Immediately remove it from relations and buckets.
  // Postpone deletion until modification_end to be sure that
  // 1) No attempt is made to re-create it.
  // 2) Parallel index is cleaned up.
  // 3) Parallel sharing is cleaned up.
  // 4) Parallel ghosting is cleaned up.
  //
  // Must clean up the parallel lists before fully deleting the entity.

  // It is important that relations be destroyed from highest to lowest rank so
  // that the back relations are destroyed first.
  for (EntityRank irank = end_rank; irank != stk::topology::BEGIN_RANK; )
  {
    --irank;
    Entity const *rel_entities = begin_entities(entity, irank);
    ConnectivityOrdinal const *rel_ordinals = begin_ordinals(entity, irank);
    for (unsigned j = num_connectivity(entity, irank); j > 0u; )
    {
      --j;
      if (is_valid(rel_entities[j])) {
        destroy_relation(entity, rel_entities[j], rel_ordinals[j]);
      }
    }
  }

  // We need to save these items and call remove_entity AFTER the call to
  // destroy_later because remove_entity may destroy the bucket
  // which would cause problems in m_entity_repo.destroy_later because it
  // makes references to the entity's original bucket.

  // Need to invalidate Entity handles in comm-list
  std::vector<EntityCommListInfo>::iterator lb_itr =
    std::lower_bound(m_entity_comm_list.begin(), m_entity_comm_list.end(), entity_key(entity));
  if (lb_itr != m_entity_comm_list.end() && lb_itr->key == entity_key(entity)) {
    lb_itr->entity = Entity();
  }

  remove_entity_callback(entity_rank(entity), bucket(entity).bucket_id(), bucket_ordinal(entity));

  m_entities_index.register_removed_key( entity_key(entity) );

  bucket(entity).getPartition()->remove(entity);
  m_entity_repo.destroy_entity(entity_key(entity), entity );
  m_entity_states[entity.local_offset()] = Deleted;
  m_deleted_entities_current_modification_cycle.push_front(entity.local_offset());

  m_check_invalid_rels = true;
  return true ;
}

//----------------------------------------------------------------------

void BulkData::generate_new_entities(const std::vector<size_t>& requests,
                                 std::vector<Entity>& requested_entities)
{
  Trace_("stk::mesh::BulkData::generate_new_entities");

  typedef stk::parallel::DistributedIndex::KeyType       KeyType;
  typedef stk::parallel::DistributedIndex::KeyTypeVector KeyTypeVector;
  typedef std::vector< KeyTypeVector > RequestKeyVector;

  RequestKeyVector requested_key_types;

  m_entities_index.generate_new_keys(requests, requested_key_types);

  //generating 'owned' entities
  Part * const owns = & m_mesh_meta_data.locally_owned_part();

  std::vector<Part*> rem ;
  std::vector<Part*> add;
  add.push_back( owns );

  requested_entities.clear();
  unsigned cnt=0;
  for (RequestKeyVector::const_iterator itr = requested_key_types.begin(); itr != requested_key_types.end(); ++itr) {
    const KeyTypeVector& key_types = *itr;
    for (KeyTypeVector::const_iterator
        kitr = key_types.begin(); kitr != key_types.end(); ++kitr) {
      ++cnt;
    }
  }
  requested_entities.reserve(cnt);

  for (RequestKeyVector::const_iterator itr = requested_key_types.begin(); itr != requested_key_types.end(); ++itr) {
    const KeyTypeVector & key_types = *itr;
    for ( KeyTypeVector::const_iterator
        kitr = key_types.begin(); kitr != key_types.end(); ++kitr) {
      EntityKey key( static_cast<EntityKey::entity_key_t>((*kitr) ));
      require_good_rank_and_id(key.rank(), key.id());
      std::pair<Entity , bool> result = m_entity_repo.internal_create_entity(key);

      //if an entity is declared with the declare_entity function in
      //the same modification cycle as the generate_new_entities
      //function, and it happens to generate a key that was declared
      //previously in the same cycle it is an error
      ThrowErrorMsgIf( ! result.second,
                       "Generated id " << key.id() <<
                       " which was already used in this modification cycle.");

      // A new application-created entity

      Entity new_entity = result.first;

      //add entity to 'owned' part
      change_entity_parts( new_entity , add , rem );
      requested_entities.push_back(new_entity);

      this->set_parallel_owner_rank( new_entity, m_parallel_rank);
      set_synchronized_count( new_entity, m_sync_count);
    }
  }
}

bool BulkData::in_shared(EntityKey key, int proc) const
{
  PairIterEntityComm sharing = entity_comm_sharing(key);
  for ( ; !sharing.empty(); ++sharing ) {
    if ( proc == sharing->proc ) {
      return true ;
    }
  }
  return false ;
}

bool BulkData::in_send_ghost( EntityKey key , int proc ) const
{
  const int owner_rank = entity_comm_owner(key);
  for ( PairIterEntityComm ec = entity_comm(key); ! ec.empty() ; ++ec ) {
    if ( ec->ghost_id != 0 &&
         ec->proc     != owner_rank &&
         ec->proc     == proc ) {
      return true;
    }
  }
  return false;
}

bool BulkData::in_ghost( const Ghosting & ghost , EntityKey key , int proc ) const
{
  // Ghost communication from owner.
  EntityCommInfo tmp( ghost.ordinal() , proc );

  PairIterEntityComm ec = entity_comm(key);
  std::vector<EntityCommInfo>::const_iterator i =
    std::lower_bound( ec.begin(), ec.end() , tmp );

  return i != ec.end() && tmp == *i ;
}

void BulkData::comm_procs( EntityKey key, std::vector<int> & procs ) const
{
  procs.clear();
  for ( PairIterEntityComm ec = entity_comm(key); ! ec.empty() ; ++ec ) {
    procs.push_back( ec->proc );
  }
  std::sort( procs.begin() , procs.end() );
  std::vector<int>::iterator
    i = std::unique( procs.begin() , procs.end() );
  procs.erase( i , procs.end() );
}

void BulkData::comm_procs( const Ghosting & ghost ,
                           EntityKey key, std::vector<int> & procs ) const
{
  procs.clear();
  for ( PairIterEntityComm ec = entity_comm(key); ! ec.empty() ; ++ec ) {
    if ( ec->ghost_id == ghost.ordinal() ) {
      procs.push_back( ec->proc );
    }
  }
}

void BulkData::internal_change_owner_in_comm_data(const EntityKey& key, int new_owner)
{
  const bool changed = m_entity_comm_map.change_owner_rank(key, new_owner);
  if (changed) {
    std::vector<EntityCommListInfo>::iterator lb_itr = std::lower_bound(m_entity_comm_list.begin(),
                                                                        m_entity_comm_list.end(),
                                                                        key);
    if (lb_itr != m_entity_comm_list.end() && lb_itr->key == key) {
      lb_itr->owner = new_owner;
    }
  }
}

void BulkData::internal_sync_comm_list_owners()
{
  for (size_t i = 0, e = m_entity_comm_list.size(); i < e; ++i) {
    m_entity_comm_list[i].owner = parallel_owner_rank(m_entity_comm_list[i].entity);
  }
}

void BulkData::new_bucket_callback(EntityRank rank, unsigned const* part_ord_begin, unsigned const* part_ord_end, size_t capacity)
{
  const std::vector< FieldBase * > & field_set = mesh_meta_data().get_fields();

  if (m_num_fields == -1) {
    // hasn't been set yet
    m_num_fields = field_set.size();
    m_field_meta_data.resize(m_num_fields * mesh_meta_data().entity_rank_count());
  }

  // Sizing loop
  size_t total_field_data_size = 0;
  for (int i = 0; i < m_num_fields; ++i) {
    FieldMetaData field_meta_data = {0, NULL, NULL};

    const FieldBase  & field = * field_set[i];
    unsigned num_bytes_per_entity = 0;

    const FieldBase::Restriction & restriction =
      find_restriction(field, rank, part_ord_begin, part_ord_end, PartOrdLess());

    if ( restriction.dimension() > 0 ) { // Exists

      const unsigned type_stride = field.data_traits().stride_of ;
      const unsigned field_rank  = field.rank();

      num_bytes_per_entity = type_stride *
        ( field_rank ? restriction.stride( field_rank - 1 ) : 1 );

      if (num_bytes_per_entity > 0) {
        field_meta_data.m_size   = num_bytes_per_entity;
        field_meta_data.m_stride = &restriction.stride(0); // JGF: why is this a pointer?

        total_field_data_size += num_bytes_per_entity * capacity;
      }
    }

    m_field_meta_data[m_num_fields * rank + i].push_back(field_meta_data);
  }

  // Allocate all field data for this bucket
  if (total_field_data_size > 0) {
    unsigned char* all_data = field_data_allocator().allocate(total_field_data_size);
    m_field_raw_data[rank].push_back(all_data);

    // Set data ptrs in field meta datas
    size_t current_field_offset = 0;
    for ( int i = 0; i < m_num_fields; ++i ) {
      FieldMetaData& field_meta_data = m_field_meta_data[m_num_fields * rank + i].back();
      const FieldBase  & field = * field_set[i];

      if (field_meta_data.m_size > 0) {
        field_meta_data.m_data = all_data + current_field_offset;
        current_field_offset += field_meta_data.m_size * capacity;

        // initialize field data
        const unsigned char* init_val = reinterpret_cast<const unsigned char*>(field.get_initial_value());
        if (init_val != NULL) {
          for (size_t j = 0; j < capacity; ++j) {
            std::memcpy( field_meta_data.m_data + j * field_meta_data.m_size, init_val, field_meta_data.m_size );
          }
        }
        else {
          std::memset( field_meta_data.m_data, 0, capacity * field_meta_data.m_size );
        }
      }
    }
  }
  else {
    m_field_raw_data[rank].push_back(NULL);
  }
}

void BulkData::copy_entity_fields_callback(EntityRank dst_rank, unsigned dst_bucket_id, unsigned dst_bucket_ord,
                                    EntityRank src_rank, unsigned src_bucket_id, unsigned src_bucket_ord)
{
  for (int i = 0; i < m_num_fields; ++i) {
    const int src_size        = m_field_meta_data[m_num_fields * src_rank + i][src_bucket_id].m_size;
    if (src_size == 0) {
      continue;
    }

    unsigned char * const src = m_field_meta_data[m_num_fields * src_rank + i][src_bucket_id].m_data;
    const int dst_size        = m_field_meta_data[m_num_fields * dst_rank + i][dst_bucket_id].m_size;

    if ( dst_size ) {
      unsigned char * const dst = m_field_meta_data[m_num_fields * dst_rank + i][dst_bucket_id].m_data;
      ThrowAssertMsg( dst_size == src_size,
                      "Incompatible field sizes: " << dst_size << " != " << src_size );

      std::swap_ranges(dst + dst_size * dst_bucket_ord,
                       dst + dst_size * dst_bucket_ord + dst_size,
                       src + src_size * src_bucket_ord);
    }
  }
}

void BulkData::remove_entity_callback(EntityRank rank, unsigned bucket_id, unsigned bucket_ord)
{
  const std::vector< FieldBase * > & field_set = mesh_meta_data().get_fields();
  for ( int i = 0; i < m_num_fields; ++i) {
    const FieldBase  & field      = *field_set[i];
    FieldMetaData field_meta_data = m_field_meta_data[m_num_fields * rank + i][bucket_id];
    const int num_bytes_per_entity = field_meta_data.m_size;

    if (num_bytes_per_entity > 0) {
      // reset field data
      const unsigned char* init_val = reinterpret_cast<const unsigned char*>(field.get_initial_value());
      if (init_val != NULL) {
        std::memcpy( field_meta_data.m_data + bucket_ord * num_bytes_per_entity, init_val, num_bytes_per_entity );
      }
      else {
        std::memset( field_meta_data.m_data + bucket_ord * num_bytes_per_entity, 0, num_bytes_per_entity );
      }
    }
  }
}

void BulkData::destroy_bucket_callback(EntityRank rank, unsigned bucket_id, unsigned capacity)
{
  if (m_field_raw_data[rank][bucket_id] != NULL) {
    size_t bytes_to_delete = 0;
    for (int i = 0; i < m_num_fields; ++i) {
      FieldMetaData& field_data = m_field_meta_data[m_num_fields * rank + i][bucket_id];
      if (field_data.m_data != NULL) {
        bytes_to_delete += field_data.m_size * capacity;
        field_data.m_size = 0;
        field_data.m_data = NULL;
      }
    }
    field_data_allocator().deallocate(m_field_raw_data[rank][bucket_id], bytes_to_delete);
    m_field_raw_data[rank][bucket_id] = NULL;
  }
}

void BulkData::update_field_data_states()
{
  const std::vector<FieldBase*> & field_set = mesh_meta_data().get_fields();
  const int num_ranks = mesh_meta_data().entity_rank_count();

  for ( int r = 0; r < num_ranks; ++r ) {
    for ( int i = 0 ; i < m_num_fields ; ) {
      const FieldBase & field = * field_set[i];
      const int outer_idx = m_num_fields * r + i;
      const int num_state = field.number_of_states();
      i += num_state ;

      if (num_state > 1) {
        for ( int b = 0, be = m_field_meta_data[outer_idx].size(); b < be; ++b) {
          if ( m_field_meta_data[outer_idx][b].m_size > 0 ) {
            unsigned char* data_last = m_field_meta_data[outer_idx][b].m_data;
            for ( int s = 1; s < num_state; ++s ) {
              std::swap(m_field_meta_data[outer_idx + s][b].m_data, data_last);
            }
            m_field_meta_data[outer_idx][b].m_data = data_last;
          }
        }
      }
    }
  }
  internal_update_fast_field_data(/* skip_onestate_fields */ true);
}

void BulkData::reorder_buckets_callback(EntityRank rank, const std::vector<unsigned>& id_map)
{
  std::vector<unsigned char*> field_raw_data(id_map.size());
  for (unsigned m = 0, e = id_map.size(); m < e; ++m) {
    field_raw_data[m] = m_field_raw_data[rank][id_map[m]];
  }
  m_field_raw_data[rank].swap(field_raw_data);

  for ( int i = 0 ; i < m_num_fields ; ++i ) {
    const int outer_idx = m_num_fields * rank + i;

    FieldMetaDataVector new_fields(id_map.size());
    for ( unsigned m = 0, e = id_map.size(); m < e; ++m ) {
      new_fields[m] = m_field_meta_data[outer_idx][id_map[m]];
    }
    new_fields.swap(m_field_meta_data[outer_idx]);
  }
}

} // namespace mesh
} // namespace stk
