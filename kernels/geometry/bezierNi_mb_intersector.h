// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "bezierNi_mb.h"
#include "bezier_hair_intersector.h"
#include "bezier_ribbon_intersector.h"
#include "bezier_curve_intersector.h"

namespace embree
{
  namespace isa
  {
    template<int M>
      struct BezierNiMBIntersector1
    {
      typedef BezierNiMB<M> Primitive;
      typedef Vec3vf<M> Vec3vfM;
      typedef LinearSpace3<Vec3vfM>LinearSpace3vfM;
      typedef CurvePrecalculations1 Precalculations;

      static __forceinline vbool<M> intersect(Ray& ray, const Primitive& prim, vfloat<M>& tNear_o)
      {
        const size_t N = prim.N;
        const vfloat4 offset_scale = vfloat4::loadu(prim.offset(N));
        const Vec3fa offset = Vec3fa(offset_scale);
        const Vec3fa scale = Vec3fa(shuffle<3,3,3,3>(offset_scale));
        const Vec3fa org1 = (ray.org-offset)*scale;
        const Vec3fa dir1 = ray.dir*scale;
        
        const LinearSpace3vfM space(vfloat<M>::load(prim.bounds_vx_x(N)), vfloat<M>::load(prim.bounds_vx_y(N)), vfloat<M>::load(prim.bounds_vx_z(N)),
                                    vfloat<M>::load(prim.bounds_vy_x(N)), vfloat<M>::load(prim.bounds_vy_y(N)), vfloat<M>::load(prim.bounds_vy_z(N)),
                                    vfloat<M>::load(prim.bounds_vz_x(N)), vfloat<M>::load(prim.bounds_vz_y(N)), vfloat<M>::load(prim.bounds_vz_z(N)));

        const Vec3vfM dir2 = xfmVector(space,Vec3vfM(dir1));
        const Vec3vfM org2 = xfmPoint (space,Vec3vfM(org1));
        const Vec3vfM rcp_dir2 = rcp_safe(dir2);

        const vfloat<M> ltime = (ray.time()-prim.time_offset(N))*prim.time_scale(N);
        const vfloat<M> vx_lower0 = vfloat<M>::load(prim.bounds_vx_lower0(N));
        const vfloat<M> vx_lower1 = vfloat<M>::load(prim.bounds_vx_lower1(N));
        const vfloat<M> vx_lower = madd(ltime,vx_lower1-vx_lower0,vx_lower0);
        const vfloat<M> vx_upper0 = vfloat<M>::load(prim.bounds_vx_upper0(N));
        const vfloat<M> vx_upper1 = vfloat<M>::load(prim.bounds_vx_upper1(N));
        const vfloat<M> vx_upper = madd(ltime,vx_upper1-vx_upper0,vx_upper0);

        const vfloat<M> vy_lower0 = vfloat<M>::load(prim.bounds_vy_lower0(N));
        const vfloat<M> vy_lower1 = vfloat<M>::load(prim.bounds_vy_lower1(N));
        const vfloat<M> vy_lower = madd(ltime,vy_lower1-vy_lower0,vy_lower0);
        const vfloat<M> vy_upper0 = vfloat<M>::load(prim.bounds_vy_upper0(N));
        const vfloat<M> vy_upper1 = vfloat<M>::load(prim.bounds_vy_upper1(N));
        const vfloat<M> vy_upper = madd(ltime,vy_upper1-vy_upper0,vy_upper0);
        
        const vfloat<M> vz_lower0 = vfloat<M>::load(prim.bounds_vz_lower0(N));
        const vfloat<M> vz_lower1 = vfloat<M>::load(prim.bounds_vz_lower1(N));
        const vfloat<M> vz_lower = madd(ltime,vz_lower1-vz_lower0,vz_lower0);
        const vfloat<M> vz_upper0 = vfloat<M>::load(prim.bounds_vz_upper0(N));
        const vfloat<M> vz_upper1 = vfloat<M>::load(prim.bounds_vz_upper1(N));
        const vfloat<M> vz_upper = madd(ltime,vz_upper1-vz_upper0,vz_upper0);
       
        const vfloat<M> t_lower_x = (vx_lower-vfloat<M>(org2.x))*vfloat<M>(rcp_dir2.x);
        const vfloat<M> t_upper_x = (vx_upper-vfloat<M>(org2.x))*vfloat<M>(rcp_dir2.x);
        const vfloat<M> t_lower_y = (vy_lower-vfloat<M>(org2.y))*vfloat<M>(rcp_dir2.y);
        const vfloat<M> t_upper_y = (vy_upper-vfloat<M>(org2.y))*vfloat<M>(rcp_dir2.y);
        const vfloat<M> t_lower_z = (vz_lower-vfloat<M>(org2.z))*vfloat<M>(rcp_dir2.z);
        const vfloat<M> t_upper_z = (vz_upper-vfloat<M>(org2.z))*vfloat<M>(rcp_dir2.z);

        const vfloat<M> tNear = max(mini(t_lower_x,t_upper_x),mini(t_lower_y,t_upper_y),mini(t_lower_z,t_upper_z),vfloat<M>(ray.tnear()));
        const vfloat<M> tFar  = min(maxi(t_lower_x,t_upper_x),maxi(t_lower_y,t_upper_y),maxi(t_lower_z,t_upper_z),vfloat<M>(ray.tfar));
        tNear_o = tNear;
        return (vint<M>(step) < vint<M>(prim.N)) & (tNear <= tFar);
      }

      static __forceinline void intersect(const Precalculations& pre, RayHit& ray, IntersectContext* context, const Primitive& prim)
      {
        Bezier1Intersector1<Curve3fa> intersectorHair;
        BezierCurve1Intersector1<Curve3fa> intersectorCurve;
        
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(normal.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time());

          if (likely(!(geom->getType() & Geometry::GTY_ROUND_CURVE)))
            intersectorHair.intersect(pre,ray,geom,a0,a1,a2,a3,Intersect1EpilogMU<VSIZEX,true>(ray,context,geomID,primID));
          else 
            intersectorCurve.intersect(pre,ray,geom,a0,a1,a2,a3,Intersect1Epilog1<true>(ray,context,geomID,primID));

          mask &= movemask(tNear <= vfloat<M>(ray.tfar));
        }
      }
      
      static __forceinline bool occluded(const Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive& prim)
      {
        Bezier1Intersector1<Curve3fa> intersectorHair;
        BezierCurve1Intersector1<Curve3fa> intersectorCurve;
        
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(shadow.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time());

          if (likely(!(geom->getType() & Geometry::GTY_ROUND_CURVE))) {
            if (intersectorHair.intersect(pre,ray,geom,a0,a1,a2,a3,Occluded1EpilogMU<VSIZEX,true>(ray,context,geomID,primID)))
              return true;
          } else {
            if (intersectorCurve.intersect(pre,ray,geom,a0,a1,a2,a3,Occluded1Epilog1<true>(ray,context,geomID,primID)))
              return true;
          }

          mask &= movemask(tNear <= vfloat<M>(ray.tfar));
        }
        return false;
      }

      template<typename Intersector, typename Epilog>
        static __forceinline void intersect(const Precalculations& pre, RayHit& ray, IntersectContext* context, const Primitive& prim)
      {
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(normal.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time());

          Intersector().intersect(pre,ray,geom,a0,a1,a2,a3,Epilog(ray,context,geomID,primID));
          mask &= movemask(tNear <= vfloat<M>(ray.tfar));
        }
      }

      template<typename Intersector, typename Epilog>
        static __forceinline bool occluded(const Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive& prim)
      {
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(shadow.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time());

          if (Intersector().intersect(pre,ray,geom,a0,a1,a2,a3,Epilog(ray,context,geomID,primID)))
              return true;

          mask &= movemask(tNear <= vfloat<M>(ray.tfar));
        }
        return false;
      }
    };

    template<int M, int K>
      struct BezierNiMBIntersectorK
    {
      typedef BezierNiMB<M> Primitive;
      typedef Vec3vf<M> Vec3vfM;
      typedef LinearSpace3<Vec3vfM>LinearSpace3vfM;
      typedef CurvePrecalculationsK<K> Precalculations;

      static __forceinline vbool<M> intersect(RayK<K>& ray, const size_t k, const Primitive& prim, vfloat<M>& tNear_o)
      {
        const size_t N = prim.N;
        const vfloat4 offset_scale = vfloat4::loadu(prim.offset(N));
        const Vec3fa offset = Vec3fa(offset_scale);
        const Vec3fa scale = Vec3fa(shuffle<3,3,3,3>(offset_scale));

        const Vec3fa ray_org(ray.org.x[k],ray.org.y[k],ray.org.z[k]);
        const Vec3fa ray_dir(ray.dir.x[k],ray.dir.y[k],ray.dir.z[k]);
        const Vec3fa org1 = (ray_org-offset)*scale;
        const Vec3fa dir1 = ray_dir*scale;
        
        const LinearSpace3vfM space(vfloat<M>::load(prim.bounds_vx_x(N)), vfloat<M>::load(prim.bounds_vx_y(N)), vfloat<M>::load(prim.bounds_vx_z(N)),
                                    vfloat<M>::load(prim.bounds_vy_x(N)), vfloat<M>::load(prim.bounds_vy_y(N)), vfloat<M>::load(prim.bounds_vy_z(N)),
                                    vfloat<M>::load(prim.bounds_vz_x(N)), vfloat<M>::load(prim.bounds_vz_y(N)), vfloat<M>::load(prim.bounds_vz_z(N)));

        const Vec3vfM dir2 = xfmVector(space,Vec3vfM(dir1));
        const Vec3vfM org2 = xfmPoint (space,Vec3vfM(org1));
        const Vec3vfM rcp_dir2 = rcp_safe(dir2);

        const vfloat<M> ltime = (ray.time()[k]-prim.time_offset(N))*prim.time_scale(N);
        const vfloat<M> vx_lower0 = vfloat<M>::load(prim.bounds_vx_lower0(N));
        const vfloat<M> vx_lower1 = vfloat<M>::load(prim.bounds_vx_lower1(N));
        const vfloat<M> vx_lower = madd(ltime,vx_lower1-vx_lower0,vx_lower0);
        const vfloat<M> vx_upper0 = vfloat<M>::load(prim.bounds_vx_upper0(N));
        const vfloat<M> vx_upper1 = vfloat<M>::load(prim.bounds_vx_upper1(N));
        const vfloat<M> vx_upper = madd(ltime,vx_upper1-vx_upper0,vx_upper0);

        const vfloat<M> vy_lower0 = vfloat<M>::load(prim.bounds_vy_lower0(N));
        const vfloat<M> vy_lower1 = vfloat<M>::load(prim.bounds_vy_lower1(N));
        const vfloat<M> vy_lower = madd(ltime,vy_lower1-vy_lower0,vy_lower0);
        const vfloat<M> vy_upper0 = vfloat<M>::load(prim.bounds_vy_upper0(N));
        const vfloat<M> vy_upper1 = vfloat<M>::load(prim.bounds_vy_upper1(N));
        const vfloat<M> vy_upper = madd(ltime,vy_upper1-vy_upper0,vy_upper0);
        
        const vfloat<M> vz_lower0 = vfloat<M>::load(prim.bounds_vz_lower0(N));
        const vfloat<M> vz_lower1 = vfloat<M>::load(prim.bounds_vz_lower1(N));
        const vfloat<M> vz_lower = madd(ltime,vz_lower1-vz_lower0,vz_lower0);
        const vfloat<M> vz_upper0 = vfloat<M>::load(prim.bounds_vz_upper0(N));
        const vfloat<M> vz_upper1 = vfloat<M>::load(prim.bounds_vz_upper1(N));
        const vfloat<M> vz_upper = madd(ltime,vz_upper1-vz_upper0,vz_upper0);
       
        const vfloat<M> t_lower_x = (vx_lower-vfloat<M>(org2.x))*vfloat<M>(rcp_dir2.x);
        const vfloat<M> t_upper_x = (vx_upper-vfloat<M>(org2.x))*vfloat<M>(rcp_dir2.x);
        const vfloat<M> t_lower_y = (vy_lower-vfloat<M>(org2.y))*vfloat<M>(rcp_dir2.y);
        const vfloat<M> t_upper_y = (vy_upper-vfloat<M>(org2.y))*vfloat<M>(rcp_dir2.y);
        const vfloat<M> t_lower_z = (vz_lower-vfloat<M>(org2.z))*vfloat<M>(rcp_dir2.z);
        const vfloat<M> t_upper_z = (vz_upper-vfloat<M>(org2.z))*vfloat<M>(rcp_dir2.z);

        const vfloat<M> tNear = max(mini(t_lower_x,t_upper_x),mini(t_lower_y,t_upper_y),mini(t_lower_z,t_upper_z),vfloat<M>(ray.tnear()[k]));
        const vfloat<M> tFar  = min(maxi(t_lower_x,t_upper_x),maxi(t_lower_y,t_upper_y),maxi(t_lower_z,t_upper_z),vfloat<M>(ray.tfar[k]));
        tNear_o = tNear;
        return (vint<M>(step) < vint<M>(prim.N)) & (tNear <= tFar);
      }

      
      static __forceinline void intersect(Precalculations& pre, RayHitK<K>& ray, const size_t k, IntersectContext* context, const Primitive& prim)
      {
        Bezier1IntersectorK<Curve3fa,K> intersectorHair;
        BezierCurve1IntersectorK<Curve3fa,K> intersectorCurve;
        
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,k,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(normal.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time()[k]);

          if (likely(!(geom->getType() & Geometry::GTY_ROUND_CURVE)))
            intersectorHair.intersect(pre,ray,k,geom,a0,a1,a2,a3,Intersect1KEpilogMU<VSIZEX,K,true>(ray,k,context,geomID,primID));
          else 
            intersectorCurve.intersect(pre,ray,k,geom,a0,a1,a2,a3,Intersect1KEpilog1<K,true>(ray,k,context,geomID,primID));

          mask &= movemask(tNear <= vfloat<M>(ray.tfar[k]));
        }
      }

      static __forceinline void intersect(const vbool<K>& valid_i, Precalculations& pre, RayHitK<K>& ray, IntersectContext* context, const Primitive& prim)
      {
        size_t mask = movemask(valid_i);
        while (mask) intersect(pre,ray,__bscf(mask),context,prim);
      }

      static __forceinline bool occluded(Precalculations& pre, RayK<K>& ray, const size_t k, IntersectContext* context, const Primitive& prim)
      {
        Bezier1IntersectorK<Curve3fa,K> intersectorHair;
        BezierCurve1IntersectorK<Curve3fa,K> intersectorCurve;
        
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,k,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(shadow.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time()[k]);

          if (likely(!(geom->getType() & Geometry::GTY_ROUND_CURVE))) {
            if (intersectorHair.intersect(pre,ray,k,geom,a0,a1,a2,a3,Occluded1KEpilogMU<VSIZEX,K,true>(ray,k,context,geomID,primID)))
              return true;
          } else {
            if (intersectorCurve.intersect(pre,ray,k,geom,a0,a1,a2,a3,Occluded1KEpilog1<K,true>(ray,k,context,geomID,primID)))
              return true;
          }

          mask &= movemask(tNear <= vfloat<M>(ray.tfar[k]));
        }
        return false;
      }

      static __forceinline vbool<K> occluded(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, IntersectContext* context, const Primitive& prim)
      {
        vbool<K> valid_o = false;
        size_t mask = movemask(valid_i);
        while (mask) {
          size_t k = __bscf(mask);
          if (occluded(pre,ray,k,context,prim))
            set(valid_o, k);
        }
        return valid_o;
      }

      template<typename Intersector, typename Epilog>
        static __forceinline void intersect(Precalculations& pre, RayHitK<K>& ray, const size_t k, IntersectContext* context, const Primitive& prim)
      {
        
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,k,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(normal.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time()[k]);

          Intersector().intersect(pre,ray,k,geom,a0,a1,a2,a3,Epilog(ray,k,context,geomID,primID));
          mask &= movemask(tNear <= vfloat<M>(ray.tfar[k]));
        }
      }

      template<typename Intersector, typename Epilog>
        static __forceinline bool occluded(Precalculations& pre, RayK<K>& ray, const size_t k, IntersectContext* context, const Primitive& prim)
      {
        vfloat<M> tNear;
        vbool<M> valid = intersect(ray,k,prim,tNear);

        const size_t N = prim.N;
        size_t mask = movemask(valid);
        while (mask)
        {
          const size_t i = __bscf(mask);
          STAT3(shadow.trav_prims,1,1,1);
          const unsigned int geomID = prim.geomID(N);
          const unsigned int primID = prim.primID(N)[i];
          const NativeCurves* geom = (NativeCurves*) context->scene->get(geomID);
          Vec3fa a0,a1,a2,a3; geom->gather(a0,a1,a2,a3,geom->curve(primID),ray.time()[k]);

          if (Intersector().intersect(pre,ray,k,geom,a0,a1,a2,a3,Epilog(ray,k,context,geomID,primID)))
            return true;

          mask &= movemask(tNear <= vfloat<M>(ray.tfar[k]));
        }
        return false;
      }
    };
  }
}
