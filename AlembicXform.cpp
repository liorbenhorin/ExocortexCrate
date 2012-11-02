#include "stdafx.h"
#include "AlembicXform.h"
#include "CommonProfiler.h"

using namespace XSI;
using namespace MATH;



void SaveXformSample(XSI::CRef parentKineStateRef, XSI::CRef kineStateRef, AbcG::OXformSchema & schema,AbcG::XformSample & sample, double time, bool xformCache, bool globalSpace, bool flattenHierarchy)
{
   

   // check if we are exporting in global space
   if(globalSpace)
   {
      if(schema.getNumSamples() > 0)
         return;

      // store identity matrix
      sample.setTranslation(Imath::V3d(0.0,0.0,0.0));
      sample.setRotation(Imath::V3d(1.0,0.0,0.0),0.0);
      sample.setScale(Imath::V3d(1.0,1.0,1.0));

      // save the sample
      schema.set(sample);
      return;
   }

   // check if the transform is animated
   //if(schema.getNumSamples() > 0)
   //{
   //   X3DObject parent(kineState.GetParent3DObject());
   //   if(!isRefAnimated(kineState.GetParent3DObject().GetRef(),xformCache))
   //      return;
   //}

   KinematicState kineState(kineStateRef);
   CTransformation globalTransform = kineState.GetTransform(time);
   CTransformation transform;

   if(flattenHierarchy){
      transform = globalTransform;
   }
   else{
      KinematicState parentKineState(parentKineStateRef);
      CMatrix4 parentGlobalTransform4 = parentKineState.GetTransform(time).GetMatrix4();
      CMatrix4 transform4 = globalTransform.GetMatrix4();
      parentGlobalTransform4.InvertInPlace();
      transform4.MulInPlace(parentGlobalTransform4);
      transform.SetMatrix4(transform4);
   }

   // store the transform
   CVector3 trans = transform.GetTranslation();
   CVector3 axis;
   double angle = transform.GetRotationAxisAngle(axis);
   CVector3 scale = transform.GetScaling();
   sample.setTranslation(Imath::V3d(trans.GetX(),trans.GetY(),trans.GetZ()));
   sample.setRotation(Imath::V3d(axis.GetX(),axis.GetY(),axis.GetZ()),RadiansToDegrees(angle));
   sample.setScale(Imath::V3d(scale.GetX(),scale.GetY(),scale.GetZ()));

   //ESS_LOG_WARNING("time: "<<time<<" trans: ("<<trans.GetX()<<", "<<trans.GetY()<<", "<<trans.GetZ()<<") angle: "<<angle<<" axis: ("<<axis.GetX()<<", "<<axis.GetY()<<", "<<axis.GetZ());

   // save the sample
   schema.set(sample);
}

ESS_CALLBACK_START( alembic_xform_Define, CRef& )
   return alembicOp_Define(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_xform_DefineLayout, CRef& )
   return alembicOp_DefineLayout(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_xform_Update, CRef& )
   ESS_PROFILE_SCOPE("alembic_xform_Update");
   OperatorContext ctxt( in_ctxt );

   if((bool)ctxt.GetParameterValue(L"muted"))
      return CStatus::OK;

   CValue udVal = ctxt.GetUserData();
   alembic_UD * p = (alembic_UD*)(CValue::siPtrType)udVal;

   double time = ctxt.GetParameterValue(L"time");
   if(p->times.size() == 0 || abs(p->lastTime - time) < 0.001)
   {
      p->times.clear();

      CString path = ctxt.GetParameterValue(L"path");
      CString identifier = ctxt.GetParameterValue(L"identifier");

     AbcG::IObject iObj = getObjectFromArchive(path,identifier);
      if(!iObj.valid())
         return CStatus::OK;
     AbcG::IXform obj(iObj,Abc::kWrapExisting);
      if(!obj.valid())
         return CStatus::OK;

     AbcG::XformSample sample;

      for(size_t i=0;i<obj.getSchema().getNumSamples();i++)
      {
		   p->times.push_back((double)obj.getSchema().getTimeSampling()->getSampleTime( i ));
		   obj.getSchema().get(sample,i);				 
         p->matrices.push_back(sample.getMatrix());
      }
   }

   Abc::M44d matrix;	// constructor creates an identity matrix

   // if no time samples, default to identity matrix
   if( p->times.size() > 0 ) {
	   // find the index
	   size_t index = p->lastFloor;
	   while(time > p->times[index] && index < p->times.size()-1)
		  index++;
	   while(time < p->times[index] && index > 0)
		  index--;

	   if(fabs(time - p->times[index]) < 0.001 || index == p->times.size()-1)
	   {
		  matrix = p->matrices[index];
	   }
	   else
	   {
		  double blend = (time - p->times[index]) / (p->times[index+1] - p->times[index]);
		  matrix = (1.0f - blend) * p->matrices[index] + blend * p->matrices[index+1];
	   }
	   p->lastFloor = index;
   }
   p->lastTime = time;
   
   CMatrix4 xsiMatrix;
   xsiMatrix.Set(
      matrix.getValue()[0],matrix.getValue()[1],matrix.getValue()[2],matrix.getValue()[3],
      matrix.getValue()[4],matrix.getValue()[5],matrix.getValue()[6],matrix.getValue()[7],
      matrix.getValue()[8],matrix.getValue()[9],matrix.getValue()[10],matrix.getValue()[11],
      matrix.getValue()[12],matrix.getValue()[13],matrix.getValue()[14],matrix.getValue()[15]);
   CTransformation xsiTransform;
   xsiTransform.SetMatrix4(xsiMatrix);

   KinematicState state(ctxt.GetOutputTarget());
   state.PutTransform(xsiTransform);

   return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_xform_Init, CRef& )
   Context ctxt( in_ctxt );
   CustomOperator op(ctxt.GetSource());

   CValue val = (CValue::siPtrType) new alembic_UD(op.GetObjectID());
   ctxt.PutUserData( val ) ;

   return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_xform_Term, CRef& )
   Context ctxt( in_ctxt );
   CustomOperator op(ctxt.GetSource());
   delRefArchive(op.GetParameterValue(L"path").GetAsText());

   CValue udVal = ctxt.GetUserData();
   alembic_UD * p = (alembic_UD*)(CValue::siPtrType)udVal;
   if(p!=NULL)
   {
      delete(p);
      p = NULL;
   }

   return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_visibility_Define, CRef& )
   return alembicOp_Define(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_visibility_DefineLayout, CRef& )
   return alembicOp_DefineLayout(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_visibility_Update, CRef& )
   ESS_PROFILE_SCOPE("alembic_visibility_Update");
   OperatorContext ctxt( in_ctxt );

   if((bool)ctxt.GetParameterValue(L"muted"))
      return CStatus::OK;

   CString path = ctxt.GetParameterValue(L"path");
   CString identifier = ctxt.GetParameterValue(L"identifier");

  AbcG::IObject obj = getObjectFromArchive(path,identifier);
   if(!obj.valid())
      return CStatus::OK;

  AbcG::IVisibilityProperty visibilityProperty = 
     AbcG::GetVisibilityProperty(obj);
   if(!visibilityProperty.valid())
      return CStatus::OK;

   SampleInfo sampleInfo = getSampleInfo(
      ctxt.GetParameterValue(L"time"),
      getTimeSamplingFromObject(obj),
      visibilityProperty.getNumSamples()
   );

   AbcA::int8_t rawVisibilityValue = visibilityProperty.getValue ( sampleInfo.floorIndex );
  AbcG::ObjectVisibility visibilityValue =AbcG::ObjectVisibility ( rawVisibilityValue );

   Property prop(ctxt.GetOutputTarget());
   switch(visibilityValue)
   {
      case AbcG::kVisibilityVisible:
      {
         prop.PutParameterValue(L"viewvis",true);
         prop.PutParameterValue(L"rendvis",true);
         break;
      }
      case AbcG::kVisibilityHidden:
      {
         prop.PutParameterValue(L"viewvis",false);
         prop.PutParameterValue(L"rendvis",false);
         break;
      }
      default:
      {
         break;
      }
   }

   return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_visibility_Term, CRef& )
   return alembicOp_Term(in_ctxt);
ESS_CALLBACK_END
