// @HEADER
// ****************************************************************************
//                Tempus: Copyright (2017) Sandia Corporation
//
// Distributed under BSD 3-clause license (See accompanying file Copyright.txt)
// ****************************************************************************
// @HEADER

#ifndef TEMPUS_TEST_BALLPARABOLIC_MODEL_IMPL_HPP
#define TEMPUS_TEST_BALLPARABOLIC_MODEL_IMPL_HPP

#include "Teuchos_StandardParameterEntryValidators.hpp"

#include "Thyra_DefaultSpmdVectorSpace.hpp"
#include "Thyra_DetachedVectorView.hpp"
#include "Thyra_DetachedMultiVectorView.hpp"
#include "Thyra_DefaultSerialDenseLinearOpWithSolveFactory.hpp"
#include "Thyra_DefaultMultiVectorLinearOpWithSolve.hpp"
#include "Thyra_DefaultLinearOpSource.hpp"
#include "Thyra_VectorStdOps.hpp"
#include <iostream>

using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::ParameterList;
using Thyra::VectorBase;


namespace Tempus_Test {

template<class Scalar>
BallParabolicModel<Scalar>::
BallParabolicModel(RCP<ParameterList> pList_)
{
  isInitialized_ = false;

  //Set up space and initial guess for solution vector
  vecLength_ = 1; 
  x_space_ = Thyra::defaultSpmdVectorSpace<Scalar>(vecLength_);
  x_vec_ = createMember(x_space_);
  Thyra::put_scalar(0.0, x_vec_.ptr());  
  x_dot_vec_ = createMember(x_space_);
  Thyra::put_scalar(1.0, x_dot_vec_.ptr()); 
  x_dot_dot_vec_ = createMember(x_space_);
  Thyra::put_scalar(0.0, x_dot_dot_vec_.ptr()); 

  //Set up responses 
  numResponses_ = 1; 
  g_space_ = Thyra::defaultSpmdVectorSpace<Scalar>(numResponses_);
    
  //Set up parameters
  //IKT: do we need parameters?  What do they mean? 
  numParameters_ = 1; 
  p_space_ = Thyra::defaultSpmdVectorSpace<Scalar>(numParameters_);
  p_init_ = createMember(p_space_); 
  Thyra::put_scalar(1.0, p_init_.ptr());   

  setParameterList(pList_);
}

template<class Scalar>
ModelEvaluatorBase::InArgs<Scalar>
BallParabolicModel<Scalar>::
getExactSolution(double t) const
{
  TEUCHOS_TEST_FOR_EXCEPTION( !isInitialized_, std::logic_error,
      "Error, setupInOutArgs_ must be called first!\n");
  ModelEvaluatorBase::InArgs<Scalar> inArgs = inArgs_;
  double exact_t = t;
  inArgs.set_t(exact_t);
  RCP<VectorBase<Scalar> > exact_x = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_x_view(*exact_x);
    exact_x_view[0] = t*(1.0-0.5*t);  
  }
  inArgs.set_x(exact_x);
  RCP<VectorBase<Scalar> > exact_x_dot = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_x_dot_view(*exact_x_dot);
    exact_x_dot_view[0] = 1.0-t;
  }
  inArgs.set_x_dot(exact_x_dot);
  RCP<VectorBase<Scalar> > exact_x_dot_dot = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_x_dot_dot_view(*exact_x_dot_dot);
    exact_x_dot_dot_view[0] = -1.0;
  }
  inArgs.set_x_dot_dot(exact_x_dot_dot);
  return(inArgs);
}

template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_x_space() const
{
  return x_space_;
}


template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_f_space() const
{
  return x_space_;
}


template<class Scalar>
ModelEvaluatorBase::InArgs<Scalar>
BallParabolicModel<Scalar>::
getNominalValues() const
{
  TEUCHOS_TEST_FOR_EXCEPTION( !isInitialized_, std::logic_error,
      "Error, setupInOutArgs_ must be called first!\n");
  return nominalValues_;
}


template<class Scalar>
RCP<Thyra::LinearOpWithSolveBase<Scalar> >
BallParabolicModel<Scalar>::
create_W() const
{
  RCP<const Thyra::LinearOpWithSolveFactoryBase<Scalar> > W_factory = this->get_W_factory();
  RCP<Thyra::LinearOpBase<Scalar> > matrix = this->create_W_op();
  RCP<Thyra::MultiVectorBase<Scalar> > matrix_mv = Teuchos::rcp_dynamic_cast<Thyra::MultiVectorBase<Scalar> >(matrix,true);
  Thyra::DetachedMultiVectorView<Scalar> matrix_view( *matrix_mv );
  //IKT: is it necessary for W to be non-singular when initialized? 
  matrix_view(0,0) = 1.0; 
  RCP<Thyra::LinearOpWithSolveBase<Scalar> > W =
    Thyra::linearOpWithSolve<Scalar>(*W_factory, matrix );
  return W;
}


template<class Scalar>
RCP<Thyra::LinearOpBase<Scalar> >
BallParabolicModel<Scalar>::
create_W_op() const
{
  RCP<Thyra::MultiVectorBase<Scalar> > matrix = Thyra::createMembers(x_space_, vecLength_);
  return(matrix);
}


template<class Scalar>
RCP<const Thyra::LinearOpWithSolveFactoryBase<Scalar> >
BallParabolicModel<Scalar>::
get_W_factory() const
{
  RCP<Thyra::LinearOpWithSolveFactoryBase<Scalar> > W_factory =
    Thyra::defaultSerialDenseLinearOpWithSolveFactory<Scalar>();
  return W_factory;
}


template<class Scalar>
ModelEvaluatorBase::InArgs<Scalar>
BallParabolicModel<Scalar>::
createInArgs() const
{
  setupInOutArgs_();
  return inArgs_;
}


// Private functions overridden from ModelEvaluatorDefaultBase


template<class Scalar>
ModelEvaluatorBase::OutArgs<Scalar>
BallParabolicModel<Scalar>::
createOutArgsImpl() const
{
  setupInOutArgs_();
  return outArgs_;
}


template<class Scalar>
void
BallParabolicModel<Scalar>::
evalModelImpl(
  const ModelEvaluatorBase::InArgs<Scalar> &inArgs,
  const ModelEvaluatorBase::OutArgs<Scalar> &outArgs
  ) const
{
  TEUCHOS_TEST_FOR_EXCEPTION( !isInitialized_, std::logic_error,
      "Error, setupInOutArgs_ must be called first!\n");

  // Parse InArgs
  RCP<const VectorBase<Scalar> > p_in = inArgs.get_p(0);
  if (!p_in.get()) std::cout << "\n ERROR: BallParabolicModel requires p as InArgs. \n";
  Thyra::ConstDetachedVectorView<Scalar> p_in_view( *p_in );
  
  RCP<const VectorBase<Scalar> > x_in = inArgs.get_x();
  if (!x_in.get()) std::cout << "\n ERROR: BallParabolicModel requires x as InArgs.  \n";
  Thyra::ConstDetachedVectorView<Scalar> x_in_view( *x_in );
  //IKT, FIXME: check that subDim() is the write routine to get local length of a Thyra::ConstDetachedVectorView 
  auto myVecLength  = x_in_view.subDim();

  RCP<const VectorBase<Scalar> > x_dot_in = inArgs.get_x_dot();
  Thyra::ConstDetachedVectorView<Scalar> x_dot_in_view( *x_dot_in);
  double alpha = inArgs.get_alpha();

  RCP<const VectorBase<Scalar> > x_dotdot_in = inArgs.get_x_dot_dot();
  Thyra::ConstDetachedVectorView<Scalar> x_dotdot_in_view( *x_dotdot_in);
  double omega = inArgs.get_W_x_dot_dot_coeff();

  //Parse OutArgs
  RCP<VectorBase<Scalar> > f_out = outArgs.get_f();
  RCP<VectorBase<Scalar> > g_out = outArgs.get_g(0);
  const RCP<Thyra::LinearOpBase<Scalar> > W_out = outArgs.get_W_op(); 

  //Populate residual and Jacobian
  //IKT: what is damping for?? 
  double damping = 0.0;
  if (f_out != Teuchos::null) {
    Thyra::DetachedVectorView<Scalar> f_out_view( *f_out );
    for (int i=0; i<myVecLength; i++) {
      f_out_view[i] = -p_in_view[0];
    }
    if (x_dotdot_in != Teuchos::null) {
      for (int i=0; i<myVecLength; i++) {
        f_out_view[i] = x_dotdot_in_view[i] - f_out_view[i];
      }
    }
    if (x_dot_in != Teuchos::null) {
      for (int i=0; i<myVecLength; i++) {
        f_out_view[i] += damping*x_dot_in_view[i];
      }
    }
  }

  // Note: W = alpha*df/dxdot + beta*df/dx + omega*df/dxdotdot
  if (W_out != Teuchos::null) {
    RCP<Thyra::MultiVectorBase<Scalar> > matrix = Teuchos::rcp_dynamic_cast<Thyra::MultiVectorBase<Scalar> >(W_out,true);
    Thyra::DetachedMultiVectorView<Scalar> matrix_view( *matrix );
    if (omega == 0.0) throw "omega = 0.0";
    matrix_view(0,0) = omega; 
    if (x_dot_in != Teuchos::null) {
      double da = damping*alpha;
      matrix_view(0,0) += da; 
    }
  }

  //Calculated response(s) g
  //g = mean value of x  
  if (g_out != Teuchos::null) {
    Thyra::DetachedVectorView<Scalar> g_out_view(*g_out);
    g_out_view[0] = Thyra::sum(*x_in)/vecLength_; 
  }
}

template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_p_space(int l) const
{
  TEUCHOS_TEST_FOR_EXCEPTION(l != 0, std::logic_error,
                     "\n Error!  BallParabolicModel::get_p_space() only " <<
                     " supports 1 parameter vector.  Supplied index l = " <<
                     l << "\n");
  return p_space_;
}

template<class Scalar>
RCP<const Teuchos::Array<std::string> >
BallParabolicModel<Scalar>::
get_p_names(int l) const
{
  TEUCHOS_TEST_FOR_EXCEPTION(l != 0, std::logic_error,
                     "\n Error!  BallParabolicModel::get_p_names() only " <<
                     " supports 1 parameter vector.  Supplied index l = " <<
                     l << "\n");

  Thyra::ConstDetachedVectorView<Scalar> p_init_view( *p_init_ );
  //IKT, FIXME: check that subDim() is the write routine to get local length of a Thyra::ConstDetachedVectorView 
  auto num_p  = p_init_view.subDim();
  RCP<Teuchos::Array<std::string> > p_names =
      rcp(new Teuchos::Array<std::string>(num_p) );
  for (int i=0; i<num_p; i++) {
    std::stringstream ss;
    ss << "Parameter " << i;
    const std::string name = ss.str();
    (*p_names)[i] = name;
  }
  return p_names;
}

template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_g_space(int j) const
{
  TEUCHOS_TEST_FOR_EXCEPTION(j != 0, std::logic_error,
                     "\n Error!  BallParabolicModel::get_g_space() only " <<
                     " supports 1 parameter vector.  Supplied index l = " <<
                     j << "\n");
  return g_space_;
}

// private

template<class Scalar>
void
BallParabolicModel<Scalar>::
setupInOutArgs_() const
{
  if (isInitialized_) return;
  
  //Set up InArgs 
  ModelEvaluatorBase::InArgsSetup<Scalar> inArgs;
  inArgs.setModelEvalDescription(this->description());
  inArgs.set_Np(numParameters_);
  inArgs.setSupports(ModelEvaluatorBase::IN_ARG_x);
  inArgs.setSupports(ModelEvaluatorBase::IN_ARG_x_dot);
  inArgs.setSupports(ModelEvaluatorBase::IN_ARG_x_dot_dot);
  inArgs.setSupports(ModelEvaluatorBase::IN_ARG_t);
  inArgs.setSupports(ModelEvaluatorBase::IN_ARG_W_x_dot_dot_coeff);
  inArgs.setSupports(ModelEvaluatorBase::IN_ARG_alpha);
  inArgs.setSupports(ModelEvaluatorBase::IN_ARG_beta);
  inArgs_ = inArgs;

  //Set up OutArgs
  ModelEvaluatorBase::OutArgsSetup<Scalar> outArgs;
  outArgs.setModelEvalDescription(this->description());
  outArgs.set_Np_Ng(numParameters_, numResponses_);

  outArgs.setSupports(ModelEvaluatorBase::OUT_ARG_f);
  outArgs.setSupports(ModelEvaluatorBase::OUT_ARG_W_op);
  //outArgs.setSupports(OUT_ARG_W,true);
  //IKT, what is the following supposed to do?? 
  //outArgs.set_W_properties( DerivativeProperties(
  //    DERIV_LINEARITY_UNKNOWN, DERIV_RANK_FULL, true));
  outArgs_ = outArgs; 
 
  // Set up nominal values
  nominalValues_ = inArgs_;
  nominalValues_.set_t(0.0);
  nominalValues_.set_x(x_vec_);
  nominalValues_.set_x_dot(x_dot_vec_);
  nominalValues_.set_x_dot_dot(x_dot_dot_vec_);
  
  isInitialized_ = true; 

}

template<class Scalar>
void
BallParabolicModel<Scalar>::
setParameterList(RCP<ParameterList> const& paramList)
{
  using Teuchos::get;
  RCP<ParameterList> tmpPL = rcp(new ParameterList("BallParabolicModel"));
  if (paramList != Teuchos::null) tmpPL = paramList;
  tmpPL->validateParametersAndSetDefaults(*this->getValidParameters());
  this->setMyParamList(tmpPL);
  RCP<ParameterList> pl = this->getMyNonconstParamList();
  setupInOutArgs_();
}

template<class Scalar>
RCP<const ParameterList>
BallParabolicModel<Scalar>::
getValidParameters() const
{
  static RCP<const ParameterList> validPL;
  if (is_null(validPL)) {
    RCP<ParameterList> pl = Teuchos::parameterList();
    validPL = pl;
  }
  return validPL;
}

} // namespace Tempus_Test
#endif // TEMPUS_TEST_BALLPARABOLIC_MODEL_IMPL_HPP
