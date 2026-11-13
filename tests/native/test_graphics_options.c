// T-GRAPHICS-SCALABILITY: authored-copy isolation, full restoration and bounds.
#include "../../runtime/graphics_options.h"
#include <assert.h>
#include <stdlib.h>
int main(void) {
    uint8_t source[0x1220],dest[0x1220];
    memset(source,0x55,sizeof source);
    GraphicsOptions o={0};
    gfx_overlay(dest,gfx_pack(&o)); // no writes for Original, even with uninitialized contents
    memcpy(dest,source,sizeof source);gfx_overlay(dest,gfx_pack(&o));assert(!memcmp(source,dest,sizeof source));
    for(int i=0;i<GFX_COUNT;i++)for(int v=0;v<gfx_count(i);v++) {
        GraphicsOptions p={0};assert(gfx_load_key(&p,gfx_keys[i],gfx_choice(i,v)));assert(p.value[i]==v);
        assert(gfx_value(gfx_pack(&p),i)==v);
        assert(gfx_load_key(&p,gfx_keys[i],"invalid"));assert(p.value[i]==0);
    }
    const unsigned flags[]={0x3ad,0x17c,0x492,0x3c,0xec,0x520,0x204,0x53d};
    for(int i=GFX_CLOUD;i<=GFX_FUR_BLUR;i++)o.value[i]=1;
    memcpy(dest,source,sizeof source);gfx_overlay(dest,gfx_pack(&o));
    for(unsigned i=0;i<sizeof flags/sizeof *flags;i++)assert(dest[flags[i]]==0);
    assert(dest[0x19c]==0); // AO Off also disables classic SSAO.
    o.value[GFX_SHADOWS]=1;gfx_overlay(dest,gfx_pack(&o));
    const unsigned floats[]={0x368,0x47c,0x2b0,0x2b4};
    for(unsigned i=0;i<4;i++){float f;memcpy(&f,dest+floats[i],4);assert(f==0);}
    memset(&o,0,sizeof o);memcpy(dest,source,sizeof source);gfx_overlay(dest,gfx_pack(&o));
    assert(!memcmp(dest,source,sizeof source)); // next authored copy restores all fields
    o.value[GFX_BLOOM]=999;assert(gfx_value(gfx_pack(&o),GFX_BLOOM)==0);
    for(unsigned i=0;i<sizeof source;i++)assert(source[i]==0x55);
    xcde_graphics_copy(0);xcde_graphics_copy(UINT64_MAX);xcde_graphics_copy(0x7100000001ull);
    puts("Graphics choices, authored-copy restoration and invalid-address guards: OK");
}
