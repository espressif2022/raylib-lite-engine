// SPDX-License-Identifier: Apache-2.0
#include "../main/rally_view.c"
#include <assert.h>
#include <stdio.h>
static float area;
static unsigned triangles;
static float expected_clip;
void DrawTriangle(Vector2 a,Vector2 b,Vector2 c,Color color)
{
    (void)color;
    assert(a.y<=expected_clip && b.y<=expected_clip && c.y<=expected_clip);
    area+=fabsf((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5f;
    ++triangles;
}
int main(void)
{
    rally_point2_t a={10,10,true},b={30,10,true},c={10,30,true},d={30,30,true};
    expected_clip=20;
    world_quad(WHITE,a,b,c,d,expected_clip);
    assert(fabsf(area-200)<.01f && triangles==2);
    area=0;triangles=0;expected_clip=5;
    world_quad(WHITE,a,b,c,d,expected_clip);assert(!triangles);
    expected_clip=40;a.visible=false;
    world_quad(WHITE,a,b,c,d,expected_clip);assert(!triangles);
    for(int i=0;i<=RALLY_SEGMENTS;++i){s_segment_z[i]=(float)i*2;s_road_top[i]=400;}
    s_road_top[8]=230;
    assert(world_clip(10)==400); /* Future hill must not hide a closer object. */
    assert(world_clip(30)==230);
    puts("world polygons clip to independent area; nearer crests occlude farther objects");
}
