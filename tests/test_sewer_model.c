// SPDX-License-Identifier: Apache-2.0
/* Exercise real game logic and feedback lifecycle without rendering a frame. */
#include <assert.h>
#include "../examples/sewer_labyrinth/main/game_module.c"

static unsigned plays,stops,unloads,closes;
void InitAudioDevice(void){}
bool IsAudioDeviceReady(void){return true;}
Sound LoadSound(const char *path){(void)path;return (Sound){1};}
void SetSoundVolume(Sound sound,float volume){(void)sound;(void)volume;}
void PlaySound(Sound sound){assert(sound.frameCount);++plays;}
void StopSound(Sound sound){assert(sound.frameCount);++stops;}
void UnloadSound(Sound sound){assert(sound.frameCount);++unloads;}
void CloseAudioDevice(void){++closes;}
void UnloadMosaicoWallAtlas(MosaicoWallAtlas atlas){(void)atlas;}

static float distance(sl_vec3_t a,sl_vec3_t b)
{sl_vec3_t d=vsub(a,b);return sqrtf(vdot(d,d));}

static void check_motion(void)
{
    sl_game_t g;reset_game(&g);
    for(int crouch=0;crouch<2;++crouch)for(int frame=0;frame<120;++frame){
        g.walk_weight=1;g.walk_phase=frame*2*SL_PI/120;g.crouch=crouch;g.tick=frame;
        sl_pose_t pose=character_pose(&g);
        for(int side=0;side<2;++side){
            assert(fabsf(distance(pose.hip[side],pose.knee[side])-.36f)<.0001f);
            assert(fabsf(distance(pose.knee[side],pose.ankle[side])-.36f)<.0001f);
            assert(fabsf(distance(pose.shoulder[side],pose.elbow[side])-.28f)<.0001f);
            assert(fabsf(distance(pose.elbow[side],pose.hand[side])-.28f)<.0001f);
            assert(pose.ankle[side].y>=.084f);
            float phase=fmodf(g.walk_phase/(2*SL_PI)+2+side*.5f,1);
            if(phase<.60f)assert(fabsf(pose.ankle[side].y-.085f)<.003f);
        }
    }
    for(int action=3;action<=5;++action)for(int tick=9;tick<=26;++tick){
        reset_game(&g);g.action=action;g.action_ticks=36-tick;
        g.facing=work_heading(action);
        sl_vec3_t center=work_center(action);
        g.x=center.x-sinf(g.facing)*.60f;g.z=center.z-cosf(g.facing)*.60f;
        sl_pose_t pose=character_pose(&g);
        for(int side=0;side<2;++side){
            sl_vec3_t hand=xf_point(pose.root,pose.hand[side].x,pose.hand[side].y,pose.hand[side].z);
            if(action==3){
                sl_vec3_t handle={center.x+(side?.105f:-.105f),
                    1.04f+.35f*cosf(starter_angle(&g)),15.12f+.35f*sinf(starter_angle(&g))};
                assert(distance(hand,handle)<.035f);
            }else{
                if(fabsf(hand.x-center.x)>=.035f)
                    fprintf(stderr,"contact: action=%d tick=%d side=%d hand=(%.3f,%.3f,%.3f) center=(%.3f,%.3f,%.3f)\n",
                            action,tick,side,hand.x,hand.y,hand.z,center.x,center.y,center.z);
                assert(fabsf(hand.x-center.x)<.035f);
                float y=hand.y-center.y,z=hand.z-center.z;
                assert(fabsf(sqrtf(y*y+z*z)-.18f)<.035f);
            }
        }
    }
    reset_game(&g);g.x=16.5f;g.z=2.65f;interact(&g);
    assert(!g.action&&g.signal==15); /* Do not reach across the room. */
    g.z=3.3f;interact(&g);assert(g.action==1);
    assert(!movement_ok(&g,16.8f,4.0f));
    assert(!camera_point_ok(&g,18.7f,15.2f));
}

static void check_scene_budget(void)
{
    unsigned max_faces=0,views=0;
    for(unsigned site=0;site<SL_SITES;++site)
    for(int state=0;state<2;++state)for(int z=1;z<15;++z)for(int x=1;x<16;++x)
        for(int angle=0;angle<8;++angle)for(int tilt=0;tilt<3;++tilt){
            sl_game_t g;reset_dispatch(&g,0,site,0,false);g.power=g.pumping=g.west=g.east=state;g.water=state?0:1;
            g.workshop_open=state;
            g.x=tile_center(x);g.z=tile_center(z);
            if(low_passage(g.x,g.z)){g.sneaking=true;g.crouch=1;}
            if(!movement_ok(&g,g.x,g.z))continue;
            g.yaw=angle*SL_PI*.25f;g.pitch=-.35f+tilt*.35f;place_camera(&g);
            s_cam_x=g.cam_x;s_cam_y=g.cam_y;s_cam_z=g.cam_z;
            s_sy=sinf(g.yaw);s_cy=cosf(g.yaw);s_sp=sinf(g.pitch);s_cp=cosf(g.pitch);
            add_scene(&g);add_character(&g);add_drone(&g);
            if(s_faces_dropped)fprintf(stderr,"scene overflow: site=%u state=%d cell=%d,%d angle=%d tilt=%d\n",site,state,x,z,angle,tilt);
            assert(!s_faces_dropped);++views;
            if((unsigned)s_face_count>max_faces)max_faces=s_face_count;
        }
    printf("scene budget: %u max faces across %u views\n",max_faces,views);
}

/* Fine navigation lattice tests actual actor clearance, including furniture,
 * live floor and shutters. No drone is advanced during this geometric proof. */
static unsigned char reachable[170*170];
static void flood_routes(const sl_game_t *g,int x,int z)
{
    static int queue[170*170];int head=0,tail=0;
    memset(reachable,0,sizeof(reachable));
    int start=(z*10+5)*170+x*10+5;queue[tail++]=start;reachable[start]=1;
    while(head<tail){
        int p=queue[head++],px=p%170,pz=p/170;
        const int dx[]={-1,1,0,0},dz[]={0,0,-1,1};
        for(int d=0;d<4;++d){
            int nx=px+dx[d],nz=pz+dz[d],q=nz*170+nx;
            if(nx<1||nx>=169||nz<1||nz>=169||reachable[q])continue;
            if(movement_ok(g,nx*.22f,nz*.22f)&&
               movement_ok(g,(px+nx)*.11f,(pz+nz)*.11f)){
                reachable[q]=1;queue[tail++]=q;
            }
        }
    }
}
static bool reached(int x,int z){return reachable[(z*10+5)*170+x*10+5]!=0;}
static void check_dispatches(void)
{
    sl_game_t g;
    for(unsigned m=0;m<SL_MISSIONS;++m)for(unsigned site=0;site<SL_SITES;++site)
    for(unsigned kit=0;kit<6;++kit){
        reset_dispatch(&g,m,site,kit,false);
        flood_routes(&g,8,1);assert(reached(7,1)&&reached(12,4)&&reached(4,4));
        g.action=1;finish_action(&g);g.action=2;finish_action(&g);
        flood_routes(&g,12,4);assert(reached(8,6)&&reached(2,8)&&reached(14,8));
        g.action=3;finish_action(&g);g.water=0;
        g.action=4;finish_action(&g);g.action=5;finish_action(&g);
        flood_routes(&g,8,6);assert(reached(8,14)&&reached(5,11)&&reached(11,11));
        g.action=7;finish_action(&g);g.action=9;finish_action(&g);g.action=10;finish_action(&g);
        if(m==SL_RECOVERY){g.action=6;finish_action(&g);}
        assert(mission_ready(&g));
        flood_routes(&g,8,14);assert(reached(8,1));
    }
    reset_dispatch(&g,SL_DRAINAGE,1,2,false);
    g.x=tile_center(8);g.z=tile_center(6);interact(&g);assert(g.action==3);
    finish_action(&g);assert(g.pumping&&!g.power&&!g.battery_charge);
    reset_dispatch(&g,SL_RECOVERY,0,0,false);
    g.x=tile_center(6);g.z=tile_center(8);interact(&g);assert(g.action==11);
    finish_action(&g);assert(walkable(&g,5,8)&&!walkable(&g,2,10)&&!g.west);
    reset_dispatch(&g,SL_SURVEY,0,0,false);g.water=0;g.pumping=true;
    for(int i=0;i<2;++i){
        g.x=tile_center(i?11:5);g.z=tile_center(11);interact(&g);
        assert(g.action==9+i);finish_action(&g);
    }
    assert(g.logs==3&&!mission_ready(&g));g.chart=true;assert(mission_ready(&g));
    reset_dispatch(&g,SL_RECOVERY,1,1,false);g.power=true;
    assert(!movement_ok(&g,tile_center(4),tile_center(3)));
    g.kit=0;assert(movement_ok(&g,tile_center(4),tile_center(3)));
    reset_dispatch(&g,SL_RECOVERY,0,1,false);g.pumping=true;g.z=tile_center(12);
    use_lure(&g);assert(g.lure_ticks==120&&!g.lure_charge);
    g.paused=true;update_game(&g);assert(g.lure_ticks==120);
    g.paused=false;float x=g.drone_x;
    for(int i=0;i<120;++i)update_game(&g);
    assert(!g.lure_ticks&&g.drone_x==x);use_lure(&g);assert(!g.lure_ticks);
    sl_module_t s={0};reset_dispatch(&s.game,0,0,0,true);clear_input(&s);
    mosaico_host_input_v1_t e={.type=MOSAICO_HOST_INPUT_ACTION,.code=1,.pressed=true};
    input(&s,&e);input(&s,&e);assert(s.game.mission==1);
    e.code=6;input(&s,&e);assert(!s.game.briefing);
    s.game.pumping=true;s.game.water=0;s.game.west=true;update(&s);
    assert(s.game.escaped&&s.completed[1]==1);update(&s);assert(s.completed[1]==1);
    s.fire_edge=true;update(&s);assert(s.game.briefing&&s.game.mission==2&&s.game.site==0);
    s.game.briefing=false;s.game.failed=true;s.fire_edge=true;update(&s);
    assert(s.game.mission==2&&s.game.site==0&&!s.game.failed&&s.game.detected);
    puts("108 mission/site/kit routes, tool effects and dispatch lifecycle: ok");
}
static void check_service_duct(void)
{
    sl_game_t g;reset_game(&g);
    g.x=tile_center(6);g.z=tile_center(10);g.sneaking=true;g.crouch=1;
    assert(!movement_ok(&g,tile_center(6),tile_center(11)));
    g.water=0;g.sneaking=false;
    assert(!movement_ok(&g,tile_center(6),tile_center(11)));
    g.sneaking=true;g.crouch=0;
    assert(!movement_ok(&g,tile_center(6),tile_center(11)));
    g.crouch=1;assert(movement_ok(&g,tile_center(6),tile_center(11)));
    g.z=tile_center(11);g.sneaking=false;update_game(&g);
    assert(g.sneaking&&g.crouch==1&&g.passage_used);
    for(int angle=0;angle<8;++angle)for(int phase=0;phase<12;++phase){
        g.facing=angle*SL_PI*.25f;g.yaw=g.facing;g.walk_phase=phase*SL_PI/6;g.walk_weight=1;
        sl_pose_t p=character_pose(&g);
        sl_vec3_t helmet=xf_point(p.head,0,.33f,0);
        assert(helmet.y<1.40f);
        place_camera(&g);assert(g.camera_distance<=1.65f&&g.cam_y<1.40f);
        for(int side=0;side<2;++side){
            assert(fabsf(distance(p.shoulder[side],p.elbow[side])-.28f)<.0001f);
            assert(fabsf(distance(p.elbow[side],p.hand[side])-.28f)<.0001f);
        }
    }
    g.record=true;assert(!walkable(&g,8,11)&&walkable(&g,6,11));
    flood_routes(&g,5,11);assert(reached(6,10)&&reached(8,1));
    g.z=tile_center(10);g.sneaking=false;g.move=0;
    for(int frame=0;frame<30;++frame)update_game(&g);
    assert(g.crouch<.01f);
    puts("service duct: flood gate, crouch clearance, helmet, camera and alarm return ok");
}
static void check_notebook_and_salvage(void)
{
    sl_module_t s={0};reset_dispatch(&s.game,0,0,0,false);clear_input(&s);
    s.game.chart=true;s.game.logs=2;s.game.pumping=true;s.game.relay=true;update(&s);
    assert(s.learned==(1|4|16)&&s.game.knowledge==s.learned);
    mosaico_host_input_v1_t key={.type=MOSAICO_HOST_INPUT_ACTION,.code=4,.pressed=true};
    input(&s,&key);input(&s,&key);assert(s.game.journal);
    uint32_t tick=s.game.tick;float water=s.game.water,drone=s.game.drone_x;
    s.game.lure_charge=true;use_lure(&s.game);assert(s.game.lure_charge);
    for(int i=0;i<20;++i)update(&s);
    assert(s.game.tick==tick&&s.game.water==water&&s.game.drone_x==drone);
    key.pressed=false;input(&s,&key);key.pressed=true;input(&s,&key);assert(!s.game.journal);
    s.game.x=tile_center(11);s.game.z=tile_center(11);s.game.lure_charge=false;
    interact(&s.game);assert(s.game.action==13&&!s.game.salvaged&&!s.game.lure_charge);
    s.game.action_ticks=1;update(&s);assert(s.game.salvaged&&s.game.lure_charge);
    use_lure(&s.game);assert(!s.game.lure_charge);
    interact(&s.game);assert(s.game.action!=13); /* The cabinet cannot generate infinite charges. */
    reset_dispatch(&s.game,0,0,1,false);s.game.logs=2;
    s.game.x=tile_center(11);s.game.z=tile_center(11);interact(&s.game);
    assert(!s.game.action&&!s.game.salvaged&&s.game.signal==21);
    mosaico_host_input_v1_t reset={.type=MOSAICO_HOST_INPUT_CONTROL,.code=MOSAICO_HOST_CONTROL_RESET};
    input(&s,&reset);update(&s);assert(s.game.knowledge==(1|4|16)&&!s.game.salvaged);
    s.game.briefing=false;
    mosaico_host_input_v1_t tap={.type=MOSAICO_HOST_INPUT_POINTER,.track_id=9,.x=400,.y=220,.pressed=true};
    input(&s,&tap);input(&s,&tap);assert(s.game.journal);
    tap.pressed=false;input(&s,&tap);tap.pressed=true;input(&s,&tap);assert(!s.game.journal);
    puts("notebook: pause, retained knowledge, one-shot salvage and touch/key edges ok");
}
static void check_patrol_variants(void)
{
    for(unsigned site=0;site<SL_SITES;++site){
        sl_game_t g;reset_dispatch(&g,0,site,1,false);g.water=0;g.west=g.east=true;
        float start_x=0,start_z=0,previous_x=0,previous_z=0;
        unsigned scanned=0;
        for(int t=0;t<=720;++t){
            g.drone_tick=t%720;patrol_pose(&g);
            assert(movement_ok(&g,g.drone_x,g.drone_z));
            assert(patrol_turn_ticks(&g)>0&&patrol_turn_ticks(&g)<=360);
            if(t==0){start_x=g.drone_x;start_z=g.drone_z;}
            else{
                float dx=g.drone_x-previous_x,dz=g.drone_z-previous_z;
                assert(dx*dx+dz*dz<.06f*.06f); /* no jumps at corners or wrap */
            }
            previous_x=g.drone_x;previous_z=g.drone_z;
            if(t<720&&g.drone_scan)++scanned;
        }
        assert(fabsf(start_x-g.drone_x)<.001f&&fabsf(start_z-g.drone_z)<.001f);
        assert(scanned==(site_config(&g)->patrol_mode==2?180u:0u));
        g.power=true;g.drone_tick=310;patrol_pose(&g);
        g.x=tile_center(8);g.z=tile_center(12);use_lure(&g);
        float x=g.drone_x,z=g.drone_z;unsigned tick=g.drone_tick;
        for(int i=0;i<120;++i)update_drone(&g);
        assert(g.drone_x==x&&g.drone_z==z&&g.drone_tick==tick);
        update_drone(&g);assert(g.drone_tick==(tick+1)%720);
        /* Both deep recesses remain places to wait, even on the inner circuit. */
        for(int side=0;side<2;++side){
            g.x=tile_center(side?11:5);g.z=tile_center(11);g.sneaking=true;g.alert=0;g.failed=false;
            for(int i=0;i<720;++i)update_drone(&g);
            assert(!g.failed);
        }
    }
    puts("patrol variants: full circuits, bounds, continuity, scan holds, lure and cover ok");
}
static void check_expedition_records(void)
{
    sl_module_t s={0};reset_dispatch(&s.game,0,0,0,false);clear_input(&s);
    for(unsigned round=0;round<18;++round){
        unsigned mission=round%3,site=round/3;
        assert(s.game.mission==mission&&s.game.site==site);
        s.game.record=s.game.chart=s.game.west=s.game.east=s.game.pumping=true;
        s.game.logs=3;s.game.water=0;s.run_ticks=60+round;
        update(&s);assert(s.game.escaped&&s.game.new_best);
        assert(s.records[mission][site][0].wins==1);
        assert(s.records[mission][site][0].best_ticks==61+round);
        assert(s.game.best_ticks==61+round);
        update(&s);assert(s.records[mission][site][0].wins==1);
        s.fire_edge=true;update(&s);assert(s.game.briefing&&s.run_ticks==0);
        dispatch_choice(&s,6);
    }
    assert(s.game.mission==0&&s.game.site==0);
    for(int m=0;m<3;++m)assert(s.completed_sites[m]==63);
    reset_dispatch(&s.game,0,0,1,false);s.game.record=true;s.run_ticks=40;update(&s);
    assert(s.records[0][0][1].best_ticks==41&&s.records[0][0][0].best_ticks==61);
    assert(s.game.best_ticks==41&&s.game.selected_wins==1);
    sl_module_t clock={0};reset_dispatch(&clock.game,0,0,0,false);clear_input(&clock);
    for(int i=0;i<10;++i)update(&clock);
    assert(clock.game.elapsed==10);
    clock.game.paused=true;update(&clock);assert(clock.run_ticks==10);
    clock.game.paused=false;clock.game.journal=true;update(&clock);assert(clock.run_ticks==10);
    clock.game.journal=false;clock.checkpoint=clock.game;clock.has_checkpoint=true;
    clock.run_ticks=123;clock.game.failed=true;clock.fire_edge=true;update(&clock);
    assert(clock.game.elapsed==124&&clock.game.tick==11);
    clock.game.record=true;update(&clock);assert(clock.game.best_ticks==125);
    mosaico_host_input_v1_t reset={.type=MOSAICO_HOST_INPUT_CONTROL,.code=MOSAICO_HOST_CONTROL_RESET};
    input(&clock,&reset);update(&clock);assert(clock.run_ticks==0&&clock.game.best_ticks==125);
    mosaico_host_input_v1_t touch={.type=MOSAICO_HOST_INPUT_POINTER,.track_id=5,.pressed=true,.x=300,.y=430};
    input(&clock,&touch);assert(clock.game.briefing); /* Progress board is not a departure button. */
    touch.pressed=false;input(&clock,&touch);
    clock.game.briefing=false;clock.game.escaped=true;
    touch.y=310;touch.pressed=true;input(&clock,&touch);update(&clock);
    unsigned site=clock.game.site;input(&clock,&touch);assert(clock.game.briefing&&clock.game.site==site);
    touch.pressed=false;input(&clock,&touch);
    clock.game.briefing=false;clock.game.escaped=true;
    mosaico_host_input_v1_t fire={.type=MOSAICO_HOST_INPUT_ACTION,.code=6,.pressed=true};
    input(&clock,&fire);update(&clock);input(&clock,&fire);assert(clock.game.briefing);
    fire.pressed=false;input(&clock,&fire);fire.pressed=true;input(&clock,&fire);assert(!clock.game.briefing);
    puts("expedition records: all 18 outings, per-kit bests, retries and pause timing ok");
}
static void check_field_map(void)
{
    sl_module_t s={0};reset_game(&s.game);clear_input(&s);
    assert(map_known(&s.game,8,1)&&!map_known(&s.game,8,14));
    assert(!map_known(&s.game,8,0)&&!map_known(&s.game,-1,1));
    s.game.chart=true;
    assert(map_known(&s.game,2,8)&&map_known(&s.game,11,11));
    assert(!map_known(&s.game,10,13));
    s.game.x=tile_center(6);s.game.z=tile_center(10);update(&s);
    assert(s.surveyed[10]&(1u<<6));
    mosaico_host_input_v1_t e={.type=MOSAICO_HOST_INPUT_CONTROL,.code=MOSAICO_HOST_CONTROL_RESET};
    input(&s,&e);update(&s);
    assert(s.game.briefing&&map_known(&s.game,6,10)&&!map_known(&s.game,10,13));
    s.game.briefing=false;s.has_checkpoint=true;s.checkpoint=s.game;
    s.game.x=tile_center(11);s.game.z=tile_center(11);survey_local(&s);
    retry_checkpoint(&s);assert(map_known(&s.game,11,11));
    e=(mosaico_host_input_v1_t){.type=MOSAICO_HOST_INPUT_ACTION,.code=4,.pressed=true};
    input(&s,&e);e.pressed=false;input(&s,&e);assert(s.game.journal);
    uint32_t tick=s.game.tick,elapsed=s.run_ticks;
    e.code=1;e.pressed=true;input(&s,&e);input(&s,&e);update(&s);
    assert(s.game.journal_page==1&&s.game.tick==tick&&s.run_ticks==elapsed&&!s.right);
    e=(mosaico_host_input_v1_t){.type=MOSAICO_HOST_INPUT_POINTER,.track_id=91,.pressed=true,.x=310,.y=96};
    input(&s,&e);input(&s,&e);assert(s.game.journal&&s.game.journal_page==0);
    e.pressed=false;input(&s,&e);e.x=390;e.pressed=true;
    input(&s,&e);assert(s.game.journal&&s.game.journal_page==1);
    e.pressed=false;input(&s,&e);e.y=422;e.pressed=true;input(&s,&e);
    assert(!s.game.journal);
}
static void check_interaction_feedback(void)
{
    for(unsigned action=1;action<=14;++action){
        sl_game_t g;reset_game(&g);
        g.kit=0;g.water=0;
        if(action==2)g.fuse=true;
        if(action==3||action==8)g.power=true;
        if(action==6)g.west=g.east=true;
        if(action==13)g.logs=2;
        sl_vec3_t c=work_center(action);float yaw=work_heading(action);
        g.x=c.x-sinf(yaw)*work_distance(action);g.z=c.z-cosf(yaw)*work_distance(action);
        assert(interaction_target(&g)==action);
        assert(!interaction_block(&g,action));
        assert(strncmp(interaction_prompt(&g,action,0),"F  ",3)==0);
        interact(&g);assert(g.action==action&&g.action_ticks==36&&!g.mark_count);
        assert(!interaction_target(&g));
    }
    sl_game_t g;reset_game(&g);g.x=16.5f;g.z=2.65f;
    assert(interaction_target(&g)==1&&interaction_block(&g,1)==15);
    assert(strstr(interaction_prompt(&g,1,15),"CLOSER"));
    interact(&g);assert(g.signal==15&&!g.action&&!g.mark_count);
    reset_game(&g);g.x=tile_center(12);g.z=tile_center(4);
    assert(interaction_block(&g,interaction_target(&g))==9);
    interact(&g);assert(g.signal==9&&!g.action&&!g.mark_count);
    reset_game(&g);g.x=tile_center(8);g.z=tile_center(14);
    assert(interaction_target(&g)==6&&interaction_block(&g,6)==4);
    for(unsigned m=SL_DRAINAGE;m<SL_MISSIONS;++m){g.mission=m;assert(!interaction_target(&g));}
    reset_game(&g);g.x=tile_center(11);g.z=tile_center(11);g.logs=2;g.lure_charge=true;
    assert(interaction_target(&g)==13&&interaction_block(&g,13)==21);
    interact(&g);assert(g.signal==21&&!g.action&&!g.mark_count);
    g.journal=true;assert(!interaction_target(&g));
}
static void check_workshop(void)
{
    sl_game_t g;reset_game(&g);
    assert(!walkable(&g,11,5));
    g.x=tile_center(11);g.z=10.70f;
    assert(!interaction_target(&g));
    assert(!movement_ok(&g,g.x,11.1f));
    g.x=tile_center(12);g.z=tile_center(6);
    assert(movement_ok(&g,g.x,g.z));
    assert(!movement_ok(&g,28.2f,14.4f)&&!camera_point_ok(&g,28.2f,14.4f));
    assert(interaction_target(&g)==14);
    interact(&g);assert(g.action==14&&!g.workshop_open);
    for(int tick=9;tick<=26;++tick){
        sl_game_t pose_game=g;pose_game.action_ticks=36-tick;pose_game.facing=SL_PI;
        pose_game.x=27.5f;pose_game.z=13.26f+work_distance(14);
        sl_pose_t pose=character_pose(&pose_game);
        float pull=smooth(.30f,.72f,action_progress(&pose_game));
        for(int side=0;side<2;++side){
            sl_vec3_t hand=xf_point(pose.root,pose.hand[side].x,pose.hand[side].y,pose.hand[side].z);
            sl_vec3_t target={27.5f+(side?-.11f:.11f),1.34f-.18f*pull,13.26f+.04f*pull};
            assert(distance(hand,target)<.035f);
        }
    }
    for(int i=0;i<35;++i)update_game(&g);
    assert(!g.workshop_open);update_game(&g);assert(g.workshop_open);
    assert(walkable(&g,11,5)&&!walkable(&g,11,8));
    for(int step=0;step<=88;++step)
        assert(movement_ok(&g,tile_center(11),tile_center(4)+step*.05f));
    g.x=tile_center(11);g.z=tile_center(5);update_game(&g);assert(g.workshop_used);
    g.record=true;g.water=0;assert(walkable(&g,11,5)&&!walkable(&g,8,11));
    puts("workshop: inner release, hand contact, clear return lane and alarm persistence ok");
}
int main(void)
{
    check_motion();check_scene_budget();
    sl_game_t normal,quiet;
    reset_game(&normal);
    assert(normal.camera_distance>2.3f);
    assert(!movement_ok(&normal,tile_center(8),tile_center(0)));
    assert(drone_sight(tile_center(4),tile_center(14),tile_center(7),tile_center(14)));
    assert(!drone_sight(tile_center(4),tile_center(14),tile_center(4),tile_center(12)));
    normal.power=true;normal.x=tile_center(7);normal.z=tile_center(14);
    quiet=normal;quiet.sneaking=true;
    for(int i=0;i<30;++i){update_game(&normal);update_game(&quiet);}
    assert(normal.seen&&quiet.seen);
    assert(quiet.alert>0&&quiet.alert<normal.alert);
    float alert=quiet.alert;
    quiet.x=tile_center(2);quiet.z=tile_center(12);
    update_game(&quiet);
    assert(!quiet.seen&&quiet.alert<alert);
    sl_game_t cover;reset_game(&cover);cover.power=true;
    cover.x=tile_center(5);cover.z=tile_center(11);
    for(int i=0;i<720;++i){update_game(&cover);assert(!cover.seen&&!cover.failed);}
    normal.paused=true;uint16_t patrol=normal.drone_tick;
    update_game(&normal);assert(normal.drone_tick==patrol);
    normal.paused=false;
    for(int i=0;i<100&&!normal.failed;++i)update_game(&normal);
    assert(normal.failed);
    uint32_t sequence=normal.sfx_seq;
    update_game(&normal);assert(normal.sfx_seq==sequence);

    sl_module_t state={0};reset_game(&state.game);clear_input(&state);
    for(int i=1;i<SL_CUES;++i)state.sounds[i].frameCount=1;
    cue(&state.game,SL_CLICK);consume_audio(&state);consume_audio(&state);
    assert(plays==1);
    cue(&state.game,SL_CLICK);consume_audio(&state);assert(plays==2);
    cue(&state.game,SL_STEP);consume_audio(&state);assert(plays==2);
    state.game.power=true;state.game.x=tile_center(8);state.game.z=tile_center(6);
    state.game.action=3;state.game.action_ticks=1;
    update(&state);assert(state.has_checkpoint&&state.game.pumping);
    state.game.failed=true;state.game.record=true;state.forward=true;
    state.game.x=tile_center(7);state.game.z=tile_center(14);
    sequence=state.game.sfx_seq;
    state.fire_edge=true;update(&state);
    assert(!state.game.failed&&!state.game.record&&state.game.pumping);
    assert(nearby(&state.game,8,6,.1f)&&!state.forward);
    assert(state.game.sfx_seq>sequence);
    mosaico_host_input_v1_t touch={.type=MOSAICO_HOST_INPUT_POINTER,.track_id=42,.pressed=true,.x=35,.y=415};
    input(&state,&touch);input(&state,&touch);assert(state.touch_sneak);
    touch.pressed=false;input(&state,&touch);touch.pressed=true;
    input(&state,&touch);assert(!state.touch_sneak);
    mosaico_host_input_v1_t pause={.type=MOSAICO_HOST_INPUT_CONTROL,.code=MOSAICO_HOST_CONTROL_PAUSE};
    unsigned old_plays=plays;input(&state,&pause);update(&state);assert(plays==old_plays);
    assert(stops>=SL_CUES-1);
    pause.code=MOSAICO_HOST_CONTROL_RESET;input(&state,&pause);
    assert(!state.has_checkpoint&&!state.game.power&&!state.game.paused);
    assert(state.pointer_id==-1&&state.sneak_id==-1);
    shutdown(&state);assert(unloads==SL_CUES-1&&closes==1);
    check_dispatches();
    check_service_duct();
    check_notebook_and_salvage();
    check_patrol_variants();
    check_expedition_records();
    check_field_map();
    check_interaction_feedback();
    check_workshop();
    puts("sewer model and audio lifecycle: ok");
    return 0;
}
