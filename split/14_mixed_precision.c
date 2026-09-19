/* -------- Mixed precision: FP16/BF16 storage + CPU accumulation -------- */
static uint16_t h_f32_to_f16(float x){
    uint32_t u;memcpy(&u,&x,4);uint32_t s=(u>>31)&1,e=(u>>23)&255,m=u&0x7fffff;
    if(e==255)return (uint16_t)((s<<15)|0x7c00|(m?0x200:0));
    int ne=(int)e-127+15;if(ne>=31)return (uint16_t)((s<<15)|0x7c00);if(ne<=0){if(ne<-10)return (uint16_t)(s<<15);m|=0x800000;int sh=14-ne;uint32_t hm=m>>sh;uint32_t rem=m&((1u<<sh)-1);if(rem>(1u<<(sh-1))||(rem==(1u<<(sh-1))&&(hm&1)))hm++;return (uint16_t)((s<<15)|hm);}uint32_t hm=m>>13,rem=m&0x1fff;if(rem>0x1000||(rem==0x1000&&(hm&1))){hm++;if(hm==0x400){hm=0;ne++;if(ne>=31)return (uint16_t)((s<<15)|0x7c00);}}return (uint16_t)((s<<15)|((uint32_t)ne<<10)|hm);
}
static float h_f16_to_f32(uint16_t h){
    uint32_t s=(uint32_t)(h>>15)&1,e=(h>>10)&31,m=h&1023,u;
    if(!e){if(!m)u=s<<31;else{int sh=0;while(!(m&0x400)){m<<=1;sh++;}m&=1023;u=(s<<31)|((uint32_t)(127-15-sh)<<23)|(m<<13);}}
    else if(e==31)u=(s<<31)|0x7f800000|(m<<13);else u=(s<<31)|((e-15+127)<<23)|(m<<13);float x;memcpy(&x,&u,4);return x;
}
static uint16_t h_f32_to_bf16(float x){uint32_t u;memcpy(&u,&x,4);uint16_t hi=(uint16_t)(u>>16);uint32_t lo=u&0xffff;if(lo>0x8000||(lo==0x8000&&(hi&1)))hi++;return hi;}
static float h_bf16_to_f32(uint16_t h){uint32_t u=(uint32_t)h<<16;float x;memcpy(&x,&u,4);return x;}
static Value tensor_to_mixed(VM*vm,int n,Value*a,int fmt){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*t=a[0].u.handle;Value o=vsobj(),d=va(),sh=tensor_shape(vm,1,a);stput(o.u.st,"format",vs(fmt==1?"fp16":"bf16"));stput(o.u.st,"shape",sh);for(size_t i=0;i<t->n;i++)ap(d.u.a,vi(fmt==1?(long long)h_f32_to_f16((float)t->data[i]):(long long)h_f32_to_bf16((float)t->data[i])));stput(o.u.st,"data",d);return o;}
static Value tensor_to_fp16(VM*vm,int n,Value*a){return tensor_to_mixed(vm,n,a,1);}
static Value tensor_to_bf16(VM*vm,int n,Value*a){return tensor_to_mixed(vm,n,a,2);}
static Value tensor_from_mixed(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTRUCT)return vn();Value f=stget(a[0].u.st,"format"),d=stget(a[0].u.st,"data"),sh=stget(a[0].u.st,"shape");if(f.t!=VSTR||d.t!=VARR||sh.t!=VARR||sh.u.a->n<1||sh.u.a->n>8)return vn();Value vals=va();for(size_t i=0;i<d.u.a->n;i++){if(d.u.a->v[i].t!=VINT||d.u.a->v[i].u.i<0||d.u.a->v[i].u.i>65535)return vn();uint16_t z=(uint16_t)d.u.a->v[i].u.i;float x=!strcmp(f.u.s,"fp16")?h_f16_to_f32(z):(!strcmp(f.u.s,"bf16")?h_bf16_to_f32(z):NAN);if(!isfinite(x)&&!isnan(x))return vn();ap(vals.u.a,vf((double)x));}Value q[3]={vals,sh,vb(0)};return tensor_from_array(vm,3,q);}
static Value tensor_mixed_matmul(VM*vm,int n,Value*a){if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VSTRUCT)return vn();Value A=tensor_from_mixed(vm,1,&a[0]),B=tensor_from_mixed(vm,1,&a[1]);if(!tensor_handle(A)||!tensor_handle(B))return vn();return tensor_matmul(vm,2,(Value[]){A,B});}
static Value tensor_mixed_info(VM*vm,int n,Value*a){(void)vm;if(n!=0)return vn();Value o=vsobj();stput(o.u.st,"fp16",vb(1));stput(o.u.st,"bf16",vb(1));stput(o.u.st,"accumulation",vs("fp64"));stput(o.u.st,"storage_bytes",vi(2));return o;}

