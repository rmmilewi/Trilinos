/*------------------------------------------------------------------------*/
/*                 Copyright 2010 Sandia Corporation.                     */
/*  Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive   */
/*  license for use of this work by or on behalf of the U.S. Government.  */
/*  Export of this program may require a license from the                 */
/*  United States Government.                                             */
/*------------------------------------------------------------------------*/

#ifndef stk_mesh_baseImpl_EntityRepository_hpp
#define stk_mesh_baseImpl_EntityRepository_hpp

#include <stk_mesh/base/Trace.hpp>

// We will use tr1 if we can (not on PGI or pathscale); otherwise, fall back to std map.
#if defined(__PGI) || defined(__PATHSCALE__)
  #define STK_MESH_ENTITYREPOSITORY_MAP_TYPE_TR1 0
#else
  #define STK_MESH_ENTITYREPOSITORY_MAP_TYPE_TR1 0
#endif

#if STK_MESH_ENTITYREPOSITORY_MAP_TYPE_TR1
  #include <tr1/unordered_map>
#else
  #include <map>
#endif

#include <stk_mesh/base/Entity.hpp>

#include <boost/pool/pool_alloc.hpp>

namespace stk {
namespace mesh {
namespace impl {

class EntityRepository {

#if STK_MESH_ENTITYREPOSITORY_MAP_TYPE_TR1
  struct stk_entity_rep_hash : public std::unary_function< EntityKey, std::size_t >
  {
    std::size_t operator()(const EntityKey& x) const
    {
      return (std::size_t)(x);
    }
  };

  typedef std::tr1::unordered_map<EntityKey, Entity , stk_entity_rep_hash, std::equal_to<EntityKey> > EntityMap;
#else
  typedef std::map<EntityKey,Entity> EntityMap;
#endif

  public:

    typedef EntityMap::const_iterator iterator;

    EntityRepository(BulkData &mesh, bool use_pool)
      : m_mesh(mesh), m_entities(), m_use_pool(use_pool) {}

    ~EntityRepository();

    Entity get_entity( const EntityKey &key ) const;

    iterator begin() const { return m_entities.begin(); }
    iterator end() const { return m_entities.end(); }

    // Return a pair: the relevant entity, and whether it had to be created
    // or not. If there was already an active entity, the second item in the
    // will be false; otherwise it will be true (even if the Entity was present
    // but marked as destroyed).
    std::pair<Entity ,bool>
      internal_create_entity( const EntityKey & key );

    void change_entity_bucket( Bucket & b, Entity e, unsigned ordinal);

    void update_entity_key(EntityKey new_key, EntityKey old_key, Entity entity);

    void destroy_entity(EntityKey key, Entity entity);

  private:
    void internal_expunge_entity( EntityMap::iterator i);

    // Entity internal_allocate_entity(EntityKey entity_key);
    Entity allocate_entity(bool use_pool);

    BulkData &m_mesh;
    EntityMap m_entities;
    bool m_use_pool;

    //disable copy constructor and assignment operator
    EntityRepository(const EntityRepository &);
    EntityRepository & operator =(const EntityRepository &);
};

inline
void EntityRepository::destroy_entity(EntityKey key, Entity entity)
{
  m_entities.erase(key);
}

} // namespace impl
} // namespace mesh
} // namespace stk

#endif // stk_mesh_baseImpl_EntityRepository_hpp
