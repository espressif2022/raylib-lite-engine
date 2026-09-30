// SPDX-License-Identifier: Apache-2.0
/* Included after the module's portable math helpers. No allocations or GPU rig. */
typedef struct {
    sl_xf_t root,pelvis,torso,head;
    sl_vec3_t shoulder[2],elbow[2],hand[2],hip[2],knee[2],ankle[2];
} sl_pose_t;

static sl_vec3_t vadd(sl_vec3_t a,sl_vec3_t b){return (sl_vec3_t){a.x+b.x,a.y+b.y,a.z+b.z};}
static sl_vec3_t vsub(sl_vec3_t a,sl_vec3_t b){return (sl_vec3_t){a.x-b.x,a.y-b.y,a.z-b.z};}
static sl_vec3_t vmul(sl_vec3_t a,float n){return (sl_vec3_t){a.x*n,a.y*n,a.z*n};}
static float vdot(sl_vec3_t a,sl_vec3_t b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static sl_vec3_t vlerp(sl_vec3_t a,sl_vec3_t b,float t){return vadd(a,vmul(vsub(b,a),t));}
static sl_vec3_t vcross(sl_vec3_t a,sl_vec3_t b)
{return (sl_vec3_t){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static sl_vec3_t vunit(sl_vec3_t a)
{return vmul(a,1.0f/sqrtf(fmaxf(vdot(a,a),.000001f)));}

/* Two fixed-length bones; the pole chooses a consistent elbow/knee direction. */
static sl_vec3_t solve_joint(sl_vec3_t start,sl_vec3_t *end,float upper,float lower,sl_vec3_t pole)
{
    sl_vec3_t axis=vsub(*end,start);
    float distance=clampf(sqrtf(vdot(axis,axis)),.025f,upper+lower-.002f);
    axis=vunit(axis);*end=vadd(start,vmul(axis,distance));
    float along=(upper*upper-lower*lower+distance*distance)/(2*distance);
    sl_vec3_t normal=vsub(pole,vmul(axis,vdot(pole,axis)));
    if(vdot(normal,normal)<.0001f)normal=vcross(axis,(sl_vec3_t){1,0,0});
    float height=sqrtf(fmaxf(0,upper*upper-along*along));
    return vadd(vadd(start,vmul(axis,along)),vmul(vunit(normal),height));
}

/* Local -Y follows a limb from joint a to joint b. */
static sl_xf_t bone_frame(sl_vec3_t a,sl_vec3_t b)
{
    sl_vec3_t y=vunit(vsub(a,b));
    sl_vec3_t x=vcross(y,(sl_vec3_t){0,0,1});
    if(vdot(x,x)<.001f)x=vcross(y,(sl_vec3_t){0,1,0});
    x=vunit(x);sl_vec3_t z=vcross(x,y);
    sl_xf_t result={{{x.x,y.x,z.x},{x.y,y.y,z.y},{x.z,y.z,z.z}},{a.x,a.y,a.z}};
    return result;
}

static sl_vec3_t actor_local(sl_xf_t root,sl_vec3_t world)
{
    sl_vec3_t d={world.x-root.t[0],world.y-root.t[1],world.z-root.t[2]};
    return (sl_vec3_t){root.r[0][0]*d.x+root.r[1][0]*d.y+root.r[2][0]*d.z,
        root.r[0][1]*d.x+root.r[1][1]*d.y+root.r[2][1]*d.z,
        root.r[0][2]*d.x+root.r[1][2]*d.y+root.r[2][2]*d.z};
}

static sl_pose_t character_pose(const sl_game_t *g)
{
    sl_pose_t p={0};float t=action_progress(g),work=action_blend(g);
    float weight=g->walk_weight*(1-work),crouch=g->crouch*(1-work);
    float phase=g->walk_phase,breath=sinf(g->tick*.075f)*.005f;
    float body_lean=.10f*weight+.15f*crouch,twist=sinf(phase)*.045f*weight;
    bool duct=low_passage(g->x,g->z);
    if(duct)body_lean+=.12f*crouch;
    if(g->action==1||g->action==6||g->action==13)body_lean+=.38f*work*(1-smooth(.5f,.85f,t));
    else if(g->action==3)body_lean+=work*(.16f-.36f*smooth(.3f,.72f,t));
    else if(g->action==4||g->action==5){body_lean+=.12f*work;twist+=sinf(wheel_angle(g,g->action))*.08f*work;}
    else body_lean+=.08f*work;
    sl_vec3_t center=work_center(g->action);
    float heading=work_heading(g->action);
    float root_x=g->x,root_z=g->z;
    float align_x=0,align_z=0;
    if(g->action){
        align_x=center.x-sinf(heading)*work_distance(g->action)-g->x;
        align_z=center.z-cosf(heading)*work_distance(g->action)-g->z;
        root_x+=align_x*work;root_z+=align_z*work;
    }
    p.root=xf_mul(xf_trans(root_x,0,root_z),xf_roty(g->facing));
    float hip_height=.72f-.18f*crouch+fabsf(sinf(phase))*.022f*weight+breath;
    p.pelvis=xf_mul(xf_trans(0,hip_height,0),xf_rotz(-g->turn_sway*.04f));
    p.torso=xf_mul(p.pelvis,xf_mul(xf_roty(twist),xf_rotx(body_lean)));
    p.head=xf_mul(p.torso,xf_mul(xf_trans(0,.48f,0),xf_roty(g->head_turn*(1-work))));
    for(int side=0;side<2;++side){
        float sign=side?1:-1;
        p.shoulder[side]=xf_point(p.torso,sign*.235f,.39f,0);
        sl_vec3_t resting={sign*.25f,hip_height-.15f,sign*sinf(phase)*.18f*weight+.12f*crouch};
        if(duct)resting=vlerp(resting,(sl_vec3_t){sign*.30f,hip_height+.18f,.20f+sign*sinf(phase)*.055f},crouch);
        float idle=(1-weight)*(1-work)*(1-crouch);
        resting.z+=.015f*sinf(g->tick*.032f+side)*idle;
        sl_vec3_t target=resting;
        if(g->action){
            sl_vec3_t contact=center;
            if(g->action==14){
                contact.y-=.18f*smooth(.30f,.72f,t);
                contact.z+=.04f*smooth(.30f,.72f,t);
            }
            if(g->action==4||g->action==5){
                float angle=(smooth(.25f,.48f,t)*(1-smooth(.48f,.56f,t))+
                             smooth(.56f,.78f,t))*.35f*SL_PI;
                float grip=sign*.18f*cosf(angle);
                contact.x+=cosf(heading)*grip;contact.z-=sinf(heading)*grip;
                contact.y+=sign*sinf(angle)*.18f;
                float release=.03f*sinf(SL_PI*clampf((t-.48f)/.08f,0,1));
                contact.x-=sinf(heading)*release;contact.z-=cosf(heading)*release;
            }else if(g->action==3){
                contact.x+=sign*.105f;
                contact.y=1.04f+.35f*cosf(starter_angle(g));
                contact.z=15.12f+.35f*sinf(starter_angle(g));
            }else if(g->action!=1&&g->action!=6&&g->action!=13){
                contact.x+=cosf(heading)*sign*.11f;
                contact.z-=sinf(heading)*sign*.11f;
            }
            target=actor_local(p.root,contact);
            if((g->action==1||g->action==6||g->action==13)&&side==1)
                target=vlerp(target,(sl_vec3_t){.22f,.84f,.14f},smooth(.52f,.85f,t));
            if((g->action==1||g->action==6||g->action==13)&&side==0)
                target=(sl_vec3_t){-.20f,.80f,.13f};
            target=vlerp(resting,target,work);
        }
        p.hand[side]=target;
        p.elbow[side]=solve_joint(p.shoulder[side],&p.hand[side],.28f,.28f,
                                  (sl_vec3_t){sign*.7f,-.8f,-.15f});
        p.hip[side]=xf_point(p.pelvis,sign*.105f,-.01f,0);
        float u=fmodf(phase/(2*SL_PI)+2+side*.5f,1);
        float foot_z,foot_y=.085f;
        if(u<.60f)foot_z=.325f-u*(.65f/.60f);
        else{
            float flight=(u-.6f)/.4f;
            foot_z=-.325f+.65f*smooth(0,1,flight);
            foot_y+=sinf(flight*SL_PI)*.13f*weight;
        }
        p.ankle[side]=(sl_vec3_t){sign*(.105f+.025f*crouch),foot_y,foot_z*weight};
        if(g->action){
            float in_start=side?.12f:0,in_end=side?.25f:.12f;
            float out_start=.80f+side*.10f,out_end=out_start+.10f;
            float plant=smooth(in_start,in_end,t)*(1-smooth(out_start,out_end,t));
            float dx=align_x*(plant-work),dz=align_z*(plant-work);
            p.ankle[side].x+=p.root.r[0][0]*dx+p.root.r[2][0]*dz;
            p.ankle[side].z+=p.root.r[0][2]*dx+p.root.r[2][2]*dz;
            float lift=sinf(SL_PI*clampf((t-in_start)/(in_end-in_start),0,1))+
                       sinf(SL_PI*clampf((t-out_start)/(out_end-out_start),0,1));
            p.ankle[side].y+=.09f*lift*clampf(sqrtf(align_x*align_x+align_z*align_z)/.20f,0,1);
        }
        if(g->action==3)p.ankle[side].z+=(side?-.10f:.10f)*work;
        p.knee[side]=solve_joint(p.hip[side],&p.ankle[side],.36f,.36f,(sl_vec3_t){0,0,1});
    }
    return p;
}
