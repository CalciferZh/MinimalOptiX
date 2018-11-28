#include <optix_world.h>

static __device__ __inline__ uchar4 makeColor(const optix::float3& c) {
  return make_uchar4(
    static_cast<unsigned char>(__saturatef(c.z)*255.99f),  /* B */
    static_cast<unsigned char>(__saturatef(c.y)*255.99f),  /* G */
    static_cast<unsigned char>(__saturatef(c.x)*255.99f),  /* R */
    255u                                                   /* A */
  );
}
