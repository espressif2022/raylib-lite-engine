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
    rally_point2_t a={10,10,true,{0}},b={30,10,true,{0}},c={10,30,true,{0}},d={30,30,true,{0}};
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
    s_camera=(rally_camera_t){.forward_z=1,.right_x=1,.focal=240,.roll_cos=1};
    rally_point3_t near={2,0,4},far={4,0,8};
    float near_size=billboard_size(near,1),far_size=billboard_size(far,1);
    assert(fabsf(near_size-60)<.01f && fabsf(far_size-30)<.01f);
    assert(fabsf(project(NULL,near).x-project(NULL,far).x)<.01f);
    assert(billboard_size((rally_point3_t){0,0,.05f},1)==0);
    area=0;triangles=0;expected_clip=RALLY_H;
    a=project(NULL,(rally_point3_t){-.01f,-.01f,.075f});
    b=project(NULL,(rally_point3_t){.01f,-.01f,.3f});
    c=project(NULL,(rally_point3_t){-.01f,.01f,.075f});
    d=project(NULL,(rally_point3_t){.01f,.01f,.3f});
    world_quad(WHITE,a,b,c,d,RALLY_H);
    assert(triangles==2 && fabsf(area-320)<.1f);
    rally_roadside_t canyon=roadside_for(1,320),snow=roadside_for(2,420);
    assert(canyon.wall_left>3 && canyon.wall_right>2 && !canyon.fence);
    assert(snow.fence && snow.wall_left>0 && snow.wall_right>0);
    for(unsigned course=1;course<=2;++course) {
        for(int at=-1800;at<=3600;at+=30) {
            rally_roadside_t first=roadside_for(course,(float)at);
            rally_roadside_t repeat=roadside_for(course,(float)at+RALLY_TRACK_LENGTH);
            assert(first.wall_left==repeat.wall_left && first.wall_right==repeat.wall_right);
            assert(first.fence==repeat.fence);
            assert(first.wall_left>=0 && first.wall_left<=3.4f);
            assert(first.wall_right>=0 && first.wall_right<=3.4f);
        }
    }
    puts("world polygons clip to independent area; nearer crests occlude farther objects");
}
