// [XCDE] T259/T260: actual scaling implementation, including output caps and CPU-bound frames.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../../runtime/settings.h"
#include "../../runtime/quality_policy.h"
#include "../../runtime/nvn/output_scale.h"
Settings g_settings;
int xcde_present_fps(void) { return g_settings.fps; }
// output_scale's LOG uses rt_log.
void rt_log(const char *fmt, ...) { (void)fmt; }
int main(void) {
    OutScale o; output_scale_set_base(1280,720);
    g_settings.fps=30; xcde_quality_apply(&g_settings,QUALITY_POTATO);
    for (int i=1;i<=200;i++) output_scale(3840,2160,false,i*.055,50,&o);
    assert(o.sy*720>=359 && o.sy*720<=541);
    double minimum=o.sy*720; assert(minimum<400);
    // Plenty of GPU headroom despite long CPU frames must allow resolution to rise.
    for (int i=201;i<=500;i++) output_scale(3840,2160,false,i*.055,5,&o);
    assert(o.sy*720>minimum && o.sy*720<=541);
    output_scale(640,360,false,30,5,&o); assert(o.sy*720<=361);
    xcde_quality_apply(&g_settings,QUALITY_PERFORMANCE);
    output_scale(1920,1080,false,31,5,&o); assert(fabs(o.sy*720-720)<1);
    xcde_quality_apply(&g_settings,QUALITY_BALANCED);
    output_scale(1280,720,false,32,-1,&o); assert(fabs(o.sy-1)<.001);
    // 1.5x is an absolute internal resolution: 1080 lines for XCDE, independent of the window.
    g_settings.res_mode=RES_1_5;
    output_scale(1280,720,false,33,5,&o); assert(fabs(o.sy*720-1080)<.001);
    output_scale(3840,2160,false,34,5,&o); assert(fabs(o.sy*720-1080)<.001);
    puts("output scale: absolute limits, CPU bottleneck, profile switching and 1.5x passed"); return 0;
}
