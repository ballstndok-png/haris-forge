/* -------------------------------------------------------------------------
   v10.2 multiplayer/server stack
   - Reliable UDP with sequencing, selective ACK bitmap, fragmentation and
     Reno-like congestion window / RTO.
   - In-memory lobby/room manager with revisioned state sync.
   - Skill-aware matchmaking with deterministic team balancing.
   - Snapshot compression (RLE) + XOR delta encoding + prediction callback.
   - Server-authoritative anti-cheat movement validation + token bucket rate
     limiting.
   Internal subsystem memory is malloc/free based so it does not rely on VM
   GC tracing for C-only state. VM values are never retained by these handles.
   ------------------------------------------------------------------------- */
#ifndef HARIS_V91_NETSTACK
#define HARIS_V91_NETSTACK 1

#define HK_REL_UDP      0x52454455 /* REDU */
#define HK_ROOM_SERVER 0x524F4F4D /* ROOM */
#define HK_MATCHMAKER  0x4D4D4B52 /* MMKR */
#define HK_REPLICATION 0x52504C43 /* RPLC */
#define HK_ANTICHEAT   0x41434854 /* ACHT */

#define HR_PROTO_VERSION 1u
#define HR_MAGIC "HREL"
#define HR_HEADER_SIZE 32u
#define HR_FLAG_ACK_ONLY 0x01u
#define HR_ACK_BITS 32u
#define HR_DEFAULT_MTU 1200u
#define HR_MIN_MTU 576u
#define HR_MAX_MTU 1400u
#define HR_MAX_MESSAGE (256u * 1024u)
#define HR_MAX_FRAGS 255u
#define HR_MAX_PEERS 1024u
#define HR_MAX_PENDING 4096u
#define HR_MAX_RETRIES 10u
#define HR_MIN_RTO 80u
#define HR_MAX_RTO 3000u
#define HR_INITIAL_CWND 8.0
#define HR_MAX_CWND 256.0
#define HR_REASM_TIMEOUT_MS 5000ULL
#define HR_MAX_REASSEMBLIES 32u
#define HR_MAX_ROOM_ID 63u
#define HR_MAX_PLAYER_ID 63u
#define HR_MAX_ROOMS 4096u
#define HR_MAX_PLAYERS_PER_ROOM 128u
#define HR_MAX_STATE (2u * 1024u * 1024u)
#define HR_MAX_MM_PLAYERS 4096u
#define HR_MAX_MM_MATCH 64u
#define HR_MAX_PREDICT_INPUT (64u * 1024u)
#define HR_AC_MAX_TRACKS 4096u
#define HR_AC_MAX_BURST 256.0
#define HR_AC_DEFAULT_RATE 60.0
#define HR_AC_MAX_DT 0.25
#define HR_AC_MAX_SPEED 100000.0

typedef struct HReliablePkt HReliablePkt;
typedef struct HReliablePeer HReliablePeer;
typedef struct HRelReasm HRelReasm;

typedef struct HReliablePkt {
    uint32_t seq;
    uint16_t frag_id;
    uint8_t frag_idx, frag_count, retries, sent;
    uint64_t sent_ms;
    unsigned char *payload;
    size_t len;
    struct sockaddr_storage addr;
    socklen_t addrlen;
    HReliablePkt *next;
} HReliablePkt;

typedef struct HRelReasm {
    uint16_t msg_id;
    uint8_t frag_count;
    uint8_t *got;
    unsigned char **part;
    uint16_t *len;
    size_t total, got_count;
    uint64_t created_ms;
    HRelReasm *next;
} HRelReasm;

typedef struct HReliablePeer {
    struct sockaddr_storage addr;
    socklen_t addrlen;
    uint32_t conn_id;
    uint32_t next_seq;
    uint32_t peer_conn_id;
    int have_peer_conn;
    uint32_t recv_ack;
    uint32_t recv_bits;
    uint16_t next_msg_id;
    double cwnd, ssthresh;
    uint32_t inflight;
    double srtt, rttvar;
    uint32_t rto_ms;
    uint64_t last_rx_ms, last_tx_ms;
    char *host_key; int port_key;
    uint64_t acked_packets, resent_packets, lost_packets;
    HReliablePkt *pending;
    HRelReasm *reasm;
} HReliablePeer;

typedef struct HReliableUDP {
    int kind;
    HUDP *udp;
    unsigned mtu;
    uint64_t sent_packets, recv_packets, dropped_packets;
    HReliablePeer *peers;
    size_t npeers, cappeers;
} HReliableUDP;

typedef struct HRoom {
    char *id;
    char **players;
    size_t nplayers, capplayers, maxplayers;
    char *state;
    size_t state_len;
    uint64_t revision;
} HRoom;

typedef struct HRoomServer {
    int kind;
    HRoom *rooms;
    size_t nrooms, caprooms, maxrooms;
    size_t maxplayers;
    uint64_t revision;
} HRoomServer;

typedef struct HMMPlayer {
    char *id;
    double skill;
    uint64_t ticket;
    uint64_t enqueued_ms;
} HMMPlayer;

typedef struct HMatchmaker {
    int kind;
    HMMPlayer *q;
    size_t n, cap;
    int target, team_size;
    double skill_window;
    uint64_t next_ticket;
    uint64_t formed;
} HMatchmaker;

typedef struct HReplication {
    int kind;
    unsigned char *last;
    size_t last_n, cap;
    uint64_t version;
    uint64_t snapshots, deltas, fulls;
    size_t max_state;
} HReplication;

typedef struct HAcTrack {
    char *player;
    double x, y, z;
    uint64_t last_ms;
    double tokens;
    uint64_t last_seq;
    int have_pos, warned;
} HAcTrack;

typedef struct HAntiCheat {
    int kind;
    HAcTrack *v;
    size_t n, cap;
    double rate, burst, max_speed, tolerance;
} HAntiCheat;

static uint64_t hr_now_us(void){
#ifdef _WIN32
    static LARGE_INTEGER freq; static int initf=0; LARGE_INTEGER c;
    if(!initf){if(!QueryPerformanceFrequency(&freq)||!freq.QuadPart)return 0;initf=1;}
    if(!QueryPerformanceCounter(&c))return 0;
    uint64_t sec=(uint64_t)(c.QuadPart/freq.QuadPart), rem=(uint64_t)(c.QuadPart%freq.QuadPart);
    return sec*1000000ULL+(rem*1000000ULL)/(uint64_t)freq.QuadPart;
#else
    struct timespec ts; if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0)return 0;
    return (uint64_t)ts.tv_sec*1000000ULL+(uint64_t)ts.tv_nsec/1000ULL;
#endif
}
static uint64_t hr_now_ms(void){
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts; if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0)return 0;
    return (uint64_t)ts.tv_sec*1000ULL+(uint64_t)ts.tv_nsec/1000000ULL;
#endif
}
static void hr_wr16(unsigned char*p,uint16_t x){p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);}
static uint16_t hr_rd16(const unsigned char*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static void hr_wr32(unsigned char*p,uint32_t x){p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24);}
static uint32_t hr_rd32(const unsigned char*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int hr_valid_bytes(Value v,unsigned char **out,size_t *n){
    if(!out||!n)return 0;*out=NULL;*n=0;
    if(v.t==VSTR){size_t z=strlen(v.u.s);if(z>HR_MAX_MESSAGE)return 0;unsigned char*p=(unsigned char*)malloc(z?z:1);if(!p)return 0;if(z)memcpy(p,v.u.s,z);*out=p;*n=z;return 1;}
    if(v.t==VARR){if(v.u.a->n>HR_MAX_MESSAGE)return 0;size_t z=v.u.a->n;unsigned char*p=(unsigned char*)malloc(z?z:1);if(!p)return 0;for(size_t i=0;i<z;i++){Value x=v.u.a->v[i];if(x.t!=VINT||x.u.i<0||x.u.i>255){free(p);return 0;}p[i]=(unsigned char)x.u.i;}*out=p;*n=z;return 1;}
    return 0;
}
static HARIS_ALWAYS_INLINE uint64_t hr_elapsed_ms(uint64_t now,uint64_t then){return now>=then?now-then:0;}
static char *hr_strdup(const char*s){if(!s)s="";size_t n=strlen(s)+1;char*p=(char*)malloc(n);if(!p)return NULL;memcpy(p,s,n);return p;}
static int hr_name_ok(const char*s,size_t max){if(!s||!*s||strlen(s)>max)return 0;for(const unsigned char*p=(const unsigned char*)s;*p;p++)if(*p<33||*p==','||*p=='/'||*p=='\\')return 0;return 1;}
static int hr_addr_equal(const struct sockaddr_storage*a,socklen_t al,const struct sockaddr_storage*b,socklen_t bl){
    if(!a||!b||a->ss_family!=b->ss_family)return 0;
    if(a->ss_family==AF_INET){const struct sockaddr_in*x=(const struct sockaddr_in*)a,*y=(const struct sockaddr_in*)b;return x->sin_port==y->sin_port&&x->sin_addr.s_addr==y->sin_addr.s_addr;}
    if(a->ss_family==AF_INET6){const struct sockaddr_in6*x=(const struct sockaddr_in6*)a,*y=(const struct sockaddr_in6*)b;return x->sin6_port==y->sin6_port&&x->sin6_scope_id==y->sin6_scope_id&&!memcmp(&x->sin6_addr,&y->sin6_addr,sizeof x->sin6_addr);}
    if(al!=bl)return 0;return !memcmp(a,b,(size_t)al);
}
static int hr_resolve_peer(const char*host,int port,struct sockaddr_storage*out,socklen_t*olen){
    if(!host||!*host||port<1||port>65535||!out||!olen)return 0;
    char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo h={0},*r=NULL;h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_DGRAM;
    if(getaddrinfo(host,ps,&h,&r)!=0)return 0;int ok=0;for(struct addrinfo*p=r;p;p=p->ai_next){if(p->ai_addrlen>(socklen_t)sizeof *out)continue;memset(out,0,sizeof *out);memcpy(out,p->ai_addr,(size_t)p->ai_addrlen);*olen=(socklen_t)p->ai_addrlen;ok=1;break;}freeaddrinfo(r);return ok;
}
static int hr_peer_host(const struct sockaddr_storage*ss,char*host,size_t hcap,int*port){
    if(!ss||!host||hcap<2||!port)return 0;host[0]=0;*port=0;
    if(ss->ss_family==AF_INET){const struct sockaddr_in*x=(const struct sockaddr_in*)ss;if(!inet_ntop(AF_INET,&x->sin_addr,host,hcap))return 0;*port=ntohs(x->sin_port);return 1;}
    if(ss->ss_family==AF_INET6){const struct sockaddr_in6*x=(const struct sockaddr_in6*)ss;if(!inet_ntop(AF_INET6,&x->sin6_addr,host,hcap))return 0;*port=ntohs(x->sin6_port);return 1;}
    return 0;
}
static int hr_seq_newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}
static int hr_acked(uint32_t seq,uint32_t ack,uint32_t bits){if(seq==ack)return 1;if(!hr_seq_newer(ack,seq))return 0;uint32_t d=ack-seq;if(d>=1&&d<=HR_ACK_BITS)return (bits>>(d-1))&1u;return 0;}
static void hr_recv_mark(HReliablePeer*p,uint32_t seq){
    if(!p||seq==0)return;
    if(p->recv_ack==0){p->recv_ack=seq;p->recv_bits=0;return;}
    if(seq==p->recv_ack)return;
    if(hr_seq_newer(seq,p->recv_ack)){uint32_t d=seq-p->recv_ack;if(d>=HR_ACK_BITS+1u)p->recv_bits=0;else p->recv_bits=(p->recv_bits<<(d))|(1u<<(d-1));p->recv_ack=seq;return;}
    uint32_t d=p->recv_ack-seq;if(d>=1&&d<=HR_ACK_BITS)p->recv_bits|=1u<<(d-1);
}
static void hr_peer_rtt(HReliablePeer*p,double sample){
    if(!p||sample<=0||!isfinite(sample))return;
    if(p->srtt<=0){p->srtt=sample;p->rttvar=sample/2.0;}else{double e=fabs(p->srtt-sample);p->rttvar=0.75*p->rttvar+0.25*e;p->srtt=0.875*p->srtt+0.125*sample;}
    double r=p->srtt+4.0*p->rttvar;if(r<HR_MIN_RTO)r=HR_MIN_RTO;if(r>HR_MAX_RTO)r=HR_MAX_RTO;p->rto_ms=(uint32_t)r;
}
static void hr_peer_ack_process(HReliablePeer*p,uint32_t ack,uint32_t bits,uint64_t now){
    if(!p)return;HReliablePkt **pp=&p->pending;while(*pp){HReliablePkt*k=*pp;if(k->sent&&hr_acked(k->seq,ack,bits)){if(k->retries==0&&k->sent_ms)hr_peer_rtt(p,(double)hr_elapsed_ms(now,k->sent_ms));p->acked_packets++;if(p->inflight)p->inflight--;if(p->cwnd<HR_MAX_CWND){if(p->cwnd<p->ssthresh)p->cwnd+=1.0;else p->cwnd+=1.0/p->cwnd;}*pp=k->next;free(k->payload);free(k);continue;}pp=&k->next;}}
static void hr_peer_loss(HReliablePeer*p){if(!p)return;p->ssthresh=fmax(2.0,p->cwnd/2.0);p->cwnd=fmax(1.0,p->ssthresh);}
static void hr_reasm_free(HRelReasm*r){if(!r)return;if(r->part){for(size_t i=0;i<r->frag_count;i++)free(r->part[i]);free(r->part);}free(r->got);free(r->len);free(r);}
static void hr_reasm_gc(HReliablePeer*p,uint64_t now){if(!p)return;HRelReasm**pp=&p->reasm;while(*pp){HRelReasm*r=*pp;if(hr_elapsed_ms(now,r->created_ms)>HR_REASM_TIMEOUT_MS){*pp=r->next;hr_reasm_free(r);continue;}pp=&r->next;}}
static HRelReasm*hr_reasm_find(HReliablePeer*p,uint16_t id){for(HRelReasm*r=p?p->reasm:NULL;r;r=r->next)if(r->msg_id==id)return r;return NULL;}
static HRelReasm*hr_reasm_new(HReliablePeer*p,uint16_t id,uint8_t count,uint64_t now){
    if(!p||count==0)return NULL;size_t n=0;for(HRelReasm*r=p->reasm;r;r=r->next)n++;if(n>=HR_MAX_REASSEMBLIES)return NULL;
    HRelReasm*r=(HRelReasm*)calloc(1,sizeof(*r));if(!r)return NULL;r->msg_id=id;r->frag_count=count;r->created_ms=now;r->got=(uint8_t*)calloc(count,1);r->part=(unsigned char**)calloc(count,sizeof(*r->part));r->len=(uint16_t*)calloc(count,sizeof(*r->len));if(!r->got||!r->part||!r->len){hr_reasm_free(r);return NULL;}r->next=p->reasm;p->reasm=r;return r;
}
static HReliablePeer*hr_peer_key_get(HReliableUDP*r,const char*host,int port){
    if(!r||!host||port<1)return NULL;
    for(size_t i=0;i<r->npeers;i++)if(r->peers[i].host_key&&r->peers[i].port_key==port&&!strcasecmp(r->peers[i].host_key,host))return &r->peers[i];
    return NULL;
}
static int hr_secure_conn_id(HReliableUDP*r,uint32_t*out){
    if(!out)return 0;
    for(int attempt=0;attempt<8;attempt++){
        uint32_t id=0;
        if(RAND_bytes((unsigned char*)&id,sizeof(id))!=1)return 0;
        if(id==0)continue;
        int collision=0;
        for(size_t i=0;i<r->npeers;i++)if(r->peers[i].conn_id==id){collision=1;break;}
        if(!collision){*out=id;return 1;}
    }
    return 0;
}
static HReliablePeer*hr_peer_get(HReliableUDP*r,const struct sockaddr_storage*addr,socklen_t addrlen,uint32_t conn_id,int create){
    for(size_t i=0;i<r->npeers;i++){HReliablePeer*p=&r->peers[i];if(hr_addr_equal(&p->addr,p->addrlen,addr,addrlen))return p;}
    if(!create||r->npeers>=HR_MAX_PEERS)return NULL;
    uint32_t local_conn_id=conn_id;
    if(!local_conn_id&&!hr_secure_conn_id(r,&local_conn_id))return NULL;
    if(r->npeers==r->cappeers){size_t nc=r->cappeers?r->cappeers*2:8;if(nc>HR_MAX_PEERS)nc=HR_MAX_PEERS;HReliablePeer*np=(HReliablePeer*)realloc(r->peers,nc*sizeof(*np));if(!np)return NULL;r->peers=np;r->cappeers=nc;}
    HReliablePeer*p=&r->peers[r->npeers++];memset(p,0,sizeof(*p));memcpy(&p->addr,addr,(size_t)addrlen);p->addrlen=addrlen;p->conn_id=local_conn_id;p->next_seq=1;p->next_msg_id=1;p->cwnd=HR_INITIAL_CWND;p->ssthresh=32.0;p->rto_ms=300;p->last_rx_ms=hr_now_ms();return p;
}
static void hr_pkt_free_list(HReliablePkt*p){while(p){HReliablePkt*n=p->next;free(p->payload);free(p);p=n;}}
static void hr_peer_clear(HReliablePeer*p){if(!p)return;hr_pkt_free_list(p->pending);p->pending=NULL;while(p->reasm){HRelReasm*n=p->reasm->next;hr_reasm_free(p->reasm);p->reasm=n;}free(p->host_key);p->host_key=NULL;}
static void hr_udp_dtor(void*p){HReliableUDP*r=(HReliableUDP*)p;if(!r)return;for(size_t i=0;i<r->npeers;i++)hr_peer_clear(&r->peers[i]);free(r->peers);r->peers=NULL;r->npeers=r->cappeers=0;}
static int hr_send_datagram(HReliableUDP*r,HReliablePeer*p,uint32_t seq,uint16_t frag_id,uint8_t frag_idx,uint8_t frag_count,const unsigned char*data,size_t len,uint8_t flags){
    if(!r||!p||!r->udp||r->udp->s==HARIS_INVALID_SOCKET||len>r->mtu-HR_HEADER_SIZE||len>65535)return 0;
    unsigned char h[HR_HEADER_SIZE];memset(h,0,sizeof h);memcpy(h,HR_MAGIC,4);h[4]=HR_PROTO_VERSION;h[5]=flags;hr_wr16(h+6,HR_HEADER_SIZE);hr_wr32(h+8,p->conn_id);hr_wr32(h+12,seq);hr_wr32(h+16,p->recv_ack);hr_wr32(h+20,p->recv_bits);hr_wr16(h+24,frag_id);h[26]=frag_idx;h[27]=frag_count;hr_wr16(h+28,(uint16_t)len);
    size_t total=HR_HEADER_SIZE+len;if(total>INT_MAX||total>HR_MAX_MTU)return 0;unsigned char buf[HR_MAX_MTU];memcpy(buf,h,HR_HEADER_SIZE);if(len)memcpy(buf+HR_HEADER_SIZE,data,len);int rc=(int)sendto(r->udp->s,(const char*)buf,(int)total,0,(struct sockaddr*)&p->addr,p->addrlen);if(rc!=(int)total)return 0;r->sent_packets++;p->last_tx_ms=hr_now_ms();return 1;
}
static void hr_flush_peer(HReliableUDP*r,HReliablePeer*p,uint64_t now){
    if(!r||!p)return;for(HReliablePkt*k=p->pending;k;k=k->next){if(k->sent)continue;if(p->inflight>=(uint32_t)floor(p->cwnd))break;if(hr_send_datagram(r,p,k->seq,k->frag_id,k->frag_idx,k->frag_count,k->payload,k->len,0)){k->sent=1;k->sent_ms=now;p->inflight++;}}
}
static void hr_tick_peer(HReliableUDP*r,HReliablePeer*p,uint64_t now){
    if(!r||!p)return;for(HReliablePkt*k=p->pending;k;k=k->next){if(!k->sent)continue;if(hr_elapsed_ms(now,k->sent_ms)<(uint64_t)p->rto_ms)continue;if(k->retries>=HR_MAX_RETRIES){p->lost_packets++;if(p->inflight)p->inflight--;hr_peer_loss(p);k->sent=2;continue;}if(hr_send_datagram(r,p,k->seq,k->frag_id,k->frag_idx,k->frag_count,k->payload,k->len,0)){k->retries++;k->sent_ms=now;p->resent_packets++;hr_peer_loss(p);}}
    HReliablePkt**pp=&p->pending;while(*pp){HReliablePkt*k=*pp;if(k->sent==2){*pp=k->next;free(k->payload);free(k);continue;}pp=&k->next;}hr_flush_peer(r,p,now);
}
static Value net_reliable_open(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.reliable.open");if(n<1||n>2||!udp_handle(a[0]))return vn();HUDP*u=udp_handle(a[0]);if(u->s==HARIS_INVALID_SOCKET)return vn();int mtu=n==2&&a[1].t==VINT?(int)a[1].u.i:(int)HR_DEFAULT_MTU;if(mtu<HR_MIN_MTU||mtu>HR_MAX_MTU)return vn();HReliableUDP*r=(HReliableUDP*)xmalloc_dtor(sizeof(*r),hr_udp_dtor);memset(r,0,sizeof(*r));r->kind=HK_REL_UDP;r->udp=u;r->mtu=(unsigned)mtu;return (Value){.t=VHANDLE,.u.handle=r};
}
static HReliableUDP*hr_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_REL_UDP?(HReliableUDP*)v.u.handle:NULL;}
static Value net_reliable_send(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.reliable.send");
    if(n<2||n>4||!hr_handle(a[0]))return vi(0);
    HReliableUDP*r=hr_handle(a[0]);if(!r->udp||r->udp->s==HARIS_INVALID_SOCKET)return vi(0);
    unsigned char*payload=NULL;size_t len=0;if(!hr_valid_bytes(a[1],&payload,&len))return vi(0);if(len>HR_MAX_MESSAGE){free(payload);return vi(0);}
    struct sockaddr_storage ss; socklen_t sl=sizeof ss;HReliablePeer*p=NULL;const char*host=NULL;int port=0;
    if(r->udp->connected){
        if(n!=2||getpeername(r->udp->s,(struct sockaddr*)&ss,&sl)!=0){free(payload);return vi(0);}
        p=hr_peer_get(r,&ss,sl,0,1);
    }else{
        if(n!=4||a[2].t!=VSTR||a[3].t!=VINT){free(payload);return vi(0);}host=a[2].u.s;port=(int)a[3].u.i;
        if(port<1||port>65535||!hr_name_ok(host,255)){free(payload);return vi(0);}
        p=hr_peer_key_get(r,host,port);
        if(!p){if(!hr_resolve_peer(host,port,&ss,&sl)){free(payload);return vi(0);}p=hr_peer_get(r,&ss,sl,0,1);if(p){p->host_key=hr_strdup(host);p->port_key=port;}}
    }
    if(!p){free(payload);return vi(0);}
    uint64_t now=hr_now_ms();hr_tick_peer(r,p,now);
    size_t chunk=r->mtu-HR_HEADER_SIZE;if(chunk<64){free(payload);return vi(0);}size_t fc=(len+chunk-1)/chunk;if(fc==0)fc=1;if(fc>HR_MAX_FRAGS){free(payload);return vi(0);}
    size_t pending=0;for(HReliablePkt*k=p->pending;k;k=k->next)pending++;if(pending>HR_MAX_PENDING||fc>HR_MAX_PENDING-pending){free(payload);return vi(0);}
    uint16_t mid=p->next_msg_id++;if(!p->next_msg_id)p->next_msg_id=1;uint32_t first=p->next_seq;size_t off=0;
    HReliablePkt*new_head=NULL,*new_tail=NULL;
    for(size_t i=0;i<fc;i++){
        size_t take=len-off;if(take>chunk)take=chunk;HReliablePkt*k=(HReliablePkt*)calloc(1,sizeof(*k));if(!k){hr_pkt_free_list(new_head);free(payload);return vi(0);}k->seq=p->next_seq++;if(!p->next_seq)p->next_seq=1;k->frag_id=mid;k->frag_idx=(uint8_t)i;k->frag_count=(uint8_t)fc;k->len=take;k->payload=(unsigned char*)malloc(take?take:1);
        if(!k->payload){free(k);hr_pkt_free_list(new_head);free(payload);return vi(0);}if(take)memcpy(k->payload,payload+off,take);off+=take;if(new_tail)new_tail->next=k;else new_head=k;new_tail=k;
    }
    HReliablePkt**tail=&p->pending;while(*tail)tail=&(*tail)->next;*tail=new_head;
    free(payload);hr_flush_peer(r,p,hr_now_ms());return vi((long long)first);
}
static Value net_reliable_poll(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.reliable.poll");if(n<1||n>2||!hr_handle(a[0]))return vn();HReliableUDP*r=hr_handle(a[0]);int timeout=n==2&&a[1].t==VINT?(int)a[1].u.i:0;if(timeout<0)timeout=0;if(timeout>5000)timeout=5000;uint64_t deadline=hr_now_ms()+(uint64_t)timeout;
    for(;;){uint64_t now=hr_now_ms();for(size_t i=0;i<r->npeers;i++)hr_tick_peer(r,&r->peers[i],now);int wait=(int)(deadline>now?deadline-now:0);if(timeout==0)wait=0;if(wait&&!haris_socket_wait(r->udp->s,1,0,wait))return vn();if(!wait&&timeout>0&&hr_now_ms()>=deadline)return vn();unsigned char*buf=(unsigned char*)malloc(r->mtu);if(!buf)return vn();struct sockaddr_storage ss;socklen_t sl=sizeof ss;int rc=(int)recvfrom(r->udp->s,(char*)buf,(int)r->mtu,0,(struct sockaddr*)&ss,&sl);if(rc<=0){free(buf);if(timeout>0&&hr_now_ms()<deadline)continue;return vn();}if(rc<HR_HEADER_SIZE){free(buf);r->dropped_packets++;continue;}if(memcmp(buf,HR_MAGIC,4)!=0||buf[4]!=HR_PROTO_VERSION||hr_rd16(buf+6)!=HR_HEADER_SIZE){free(buf);r->dropped_packets++;continue;}uint16_t plen=hr_rd16(buf+28);if((size_t)rc!=HR_HEADER_SIZE+(size_t)plen||plen>r->mtu-HR_HEADER_SIZE){free(buf);r->dropped_packets++;continue;}uint8_t flags=buf[5],fidx=buf[26],fcnt=buf[27];uint32_t conn=hr_rd32(buf+8),seq=hr_rd32(buf+12),ack=hr_rd32(buf+16),ackbits=hr_rd32(buf+20);uint16_t mid=hr_rd16(buf+24);HReliablePeer*p=hr_peer_get(r,&ss,sl,0,1);if(!p){free(buf);r->dropped_packets++;continue;}if(!p->have_peer_conn){if(!conn){free(buf);r->dropped_packets++;continue;}p->peer_conn_id=conn;p->have_peer_conn=1;}else if(conn!=p->peer_conn_id){free(buf);r->dropped_packets++;continue;}if(!p->host_key){char kh[INET6_ADDRSTRLEN]={0};int kp=0;if(hr_peer_host(&ss,kh,sizeof kh,&kp)){p->host_key=hr_strdup(kh);p->port_key=kp;}}uint64_t ts=hr_now_ms();p->last_rx_ms=ts;hr_peer_ack_process(p,ack,ackbits,ts);
        if(flags&HR_FLAG_ACK_ONLY){free(buf);continue;}if(seq==0||fcnt==0||fidx>=fcnt||mid==0){free(buf);r->dropped_packets++;continue;}if((size_t)fcnt*(r->mtu-HR_HEADER_SIZE)>HR_MAX_MESSAGE){free(buf);r->dropped_packets++;continue;}hr_recv_mark(p,seq);HRelReasm*rm=hr_reasm_find(p,mid);if(!rm)rm=hr_reasm_new(p,mid,fcnt,ts);if(!rm){free(buf);r->dropped_packets++;continue;}if(rm->frag_count!=fcnt){free(buf);r->dropped_packets++;continue;}if(!rm->got[fidx]){rm->part[fidx]=(unsigned char*)malloc(plen?plen:1);if(plen&&!rm->part[fidx]){free(buf);continue;}if(plen)memcpy(rm->part[fidx],buf+HR_HEADER_SIZE,plen);rm->len[fidx]=plen;rm->got[fidx]=1;rm->got_count++;if(rm->total>HR_MAX_MESSAGE-(size_t)plen){HRelReasm**rp=&p->reasm;while(*rp&&*rp!=rm)rp=&(*rp)->next;if(*rp)*rp=rm->next;hr_reasm_free(rm);free(buf);r->dropped_packets++;continue;}rm->total+=plen;}
        /* ACK promptly so one-way traffic remains reliable. */
        (void)hr_send_datagram(r,p,0,0,0,0,NULL,0,HR_FLAG_ACK_ONLY);
        if(rm->got_count==rm->frag_count){size_t total=rm->total;unsigned char*outb=(unsigned char*)malloc(total?total:1);if(!outb){free(buf);continue;}size_t pos=0;for(size_t i=0;i<rm->frag_count;i++){if(!rm->got[i]||rm->len[i]>total-pos){free(outb);outb=NULL;break;}memcpy(outb+pos,rm->part[i],rm->len[i]);pos+=rm->len[i];}HRelReasm**pp=&p->reasm;while(*pp&&*pp!=rm)pp=&(*pp)->next;if(*pp)*pp=rm->next;hr_reasm_free(rm);free(buf);if(!outb)continue;r->recv_packets++;Value o=vsobj();Value bytes=net_bytes_array(outb,total);free(outb);if(bytes.t==VNULL)return vn();stput(o.u.st,"data",bytes);stput(o.u.st,"size",vi((long long)total));char phost[INET6_ADDRSTRLEN]={0};int pport=0;(void)hr_peer_host(&ss,phost,sizeof phost,&pport);stput(o.u.st,"host",vs(phost));stput(o.u.st,"port",vi(pport));stput(o.u.st,"seq",vi((long long)seq));return o;}
        free(buf);hr_reasm_gc(p,ts);
    }
}
static Value net_reliable_tick(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.reliable.tick");if(n!=1||!hr_handle(a[0]))return vb(0);HReliableUDP*r=hr_handle(a[0]);uint64_t now=hr_now_ms();for(size_t i=0;i<r->npeers;i++){hr_reasm_gc(&r->peers[i],now);hr_tick_peer(r,&r->peers[i],now);}return vb(1);
}
static Value net_reliable_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hr_handle(a[0]))return vn();HReliableUDP*r=hr_handle(a[0]);Value o=vsobj();stput(o.u.st,"mtu",vi(r->mtu));stput(o.u.st,"peers",vi((long long)r->npeers));stput(o.u.st,"sent",vi((long long)r->sent_packets));stput(o.u.st,"received",vi((long long)r->recv_packets));stput(o.u.st,"dropped",vi((long long)r->dropped_packets));Value ps=va();for(size_t i=0;i<r->npeers;i++){HReliablePeer*p=&r->peers[i];Value q=vsobj();char h[INET6_ADDRSTRLEN]={0};int pt=0;(void)hr_peer_host(&p->addr,h,sizeof h,&pt);stput(q.u.st,"host",vs(h));stput(q.u.st,"port",vi(pt));stput(q.u.st,"conn_id",vi((long long)p->conn_id));stput(q.u.st,"cwnd",vf(p->cwnd));stput(q.u.st,"rto_ms",vi(p->rto_ms));stput(q.u.st,"inflight",vi((long long)p->inflight));stput(q.u.st,"acked",vi((long long)p->acked_packets));stput(q.u.st,"resent",vi((long long)p->resent_packets));size_t qc=0;for(HReliablePkt*qq=p->pending;qq;qq=qq->next)qc++;stput(q.u.st,"lost",vi((long long)p->lost_packets));stput(q.u.st,"queued",vi((long long)qc));ap(ps.u.a,q);}stput(o.u.st,"peer_stats",ps);return o;}
static Value net_reliable_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HReliableUDP*r=hr_handle(a[0]);if(!r)return vb(0);r->kind=HK_CLOSED;hr_udp_dtor(r);return vb(1);}

static void hr_room_free(HRoom*r){if(!r)return;free(r->id);for(size_t i=0;i<r->nplayers;i++)free(r->players[i]);free(r->players);free(r->state);memset(r,0,sizeof(*r));}
static void hr_room_server_dtor(void*p){HRoomServer*s=(HRoomServer*)p;if(!s)return;for(size_t i=0;i<s->nrooms;i++)hr_room_free(&s->rooms[i]);free(s->rooms);s->rooms=NULL;s->nrooms=s->caprooms=0;}
static HRoomServer*hr_room_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_ROOM_SERVER?(HRoomServer*)v.u.handle:NULL;}
static HRoom*hr_room_find(HRoomServer*s,const char*id){if(!s||!id)return NULL;for(size_t i=0;i<s->nrooms;i++)if(!strcmp(s->rooms[i].id,id))return &s->rooms[i];return NULL;}
static int hr_room_player_index(HRoom*r,const char*id){if(!r||!id)return -1;for(size_t i=0;i<r->nplayers;i++)if(!strcmp(r->players[i],id))return (int)i;return -1;}
static Value game_room_server_create(VM*vm,int n,Value*a){(void)vm;if(n>2)return vn();size_t mr=n>=1&&a[0].t==VINT?(size_t)a[0].u.i:256;size_t mp=n==2&&a[1].t==VINT?(size_t)a[1].u.i:32;if(mr<1||mr>HR_MAX_ROOMS||mp<1||mp>HR_MAX_PLAYERS_PER_ROOM)return vn();HRoomServer*s=(HRoomServer*)xmalloc_dtor(sizeof(*s),hr_room_server_dtor);memset(s,0,sizeof(*s));s->kind=HK_ROOM_SERVER;s->maxrooms=mr;s->maxplayers=mp;return (Value){.t=VHANDLE,.u.handle=s};}
static Value game_room_create(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3)return vb(0);HRoomServer*s=hr_room_handle(a[0]);if(!s||a[1].t!=VSTR||!hr_name_ok(a[1].u.s,HR_MAX_ROOM_ID)||hr_room_find(s,a[1].u.s))return vb(0);size_t cap=n==3&&a[2].t==VINT?(size_t)a[2].u.i:s->maxplayers;if(cap<1||cap>s->maxplayers)return vb(0);if(s->nrooms>=s->maxrooms)return vb(0);if(s->nrooms==s->caprooms){size_t nc=s->caprooms?s->caprooms*2:16;if(nc>s->maxrooms)nc=s->maxrooms;HRoom*nr=(HRoom*)realloc(s->rooms,nc*sizeof(*nr));if(!nr)return vb(0);s->rooms=nr;s->caprooms=nc;}HRoom*r=&s->rooms[s->nrooms++];memset(r,0,sizeof(*r));r->id=hr_strdup(a[1].u.s);if(!r->id){s->nrooms--;return vb(0);}r->maxplayers=cap;r->revision=++s->revision;return vb(1);}
static Value game_room_join(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hr_room_handle(a[0])||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);HRoomServer*s=hr_room_handle(a[0]);if(!hr_name_ok(a[1].u.s,HR_MAX_ROOM_ID)||!hr_name_ok(a[2].u.s,HR_MAX_PLAYER_ID))return vb(0);HRoom*r=hr_room_find(s,a[1].u.s);if(!r||r->nplayers>=r->maxplayers||hr_room_player_index(r,a[2].u.s)>=0)return vb(0);if(r->nplayers==r->capplayers){size_t nc=r->capplayers?r->capplayers*2:8;if(nc>r->maxplayers)nc=r->maxplayers;char**np=(char**)realloc(r->players,nc*sizeof(*np));if(!np)return vb(0);r->players=np;r->capplayers=nc;}r->players[r->nplayers++]=hr_strdup(a[2].u.s);if(!r->players[r->nplayers-1]){r->nplayers--;return vb(0);}r->revision=++s->revision;return vb(1);}
static Value game_room_leave(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hr_room_handle(a[0])||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);HRoomServer*s=hr_room_handle(a[0]);HRoom*r=hr_room_find(s,a[1].u.s);if(!r)return vb(0);int ix=hr_room_player_index(r,a[2].u.s);if(ix<0)return vb(0);free(r->players[ix]);for(size_t i=(size_t)ix+1;i<r->nplayers;i++)r->players[i-1]=r->players[i];r->nplayers--;r->revision=++s->revision;return vb(1);}
static Value game_room_players(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hr_room_handle(a[0])||a[1].t!=VSTR)return vn();HRoom*r=hr_room_find(hr_room_handle(a[0]),a[1].u.s);if(!r)return vn();Value out=va();for(size_t i=0;i<r->nplayers;i++)ap(out.u.a,vs(r->players[i]));return out;}
static Value game_room_state_set(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hr_room_handle(a[0])||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);HRoomServer*s=hr_room_handle(a[0]);HRoom*r=hr_room_find(s,a[1].u.s);if(!r||strlen(a[2].u.s)>HR_MAX_STATE)return vb(0);char*cp=hr_strdup(a[2].u.s);if(!cp)return vb(0);free(r->state);r->state=cp;r->state_len=strlen(cp);r->revision=++s->revision;return vb(1);}
static Value game_room_state_get(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hr_room_handle(a[0])||a[1].t!=VSTR)return vn();HRoom*r=hr_room_find(hr_room_handle(a[0]),a[1].u.s);return r?vs(r->state?r->state:""):vn();}
static Value game_room_state_sync(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!hr_room_handle(a[0])||a[1].t!=VSTR)return vn();HRoom*r=hr_room_find(hr_room_handle(a[0]),a[1].u.s);if(!r)return vn();uint64_t known=n==3&&a[2].t==VINT&&a[2].u.i>=0?(uint64_t)a[2].u.i:0;Value o=vsobj();stput(o.u.st,"changed",vb(r->revision!=known));stput(o.u.st,"revision",vi((long long)r->revision));if(r->revision!=known)stput(o.u.st,"state",vs(r->state?r->state:""));else stput(o.u.st,"state",vn());return o;}
static Value game_room_server_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hr_room_handle(a[0]))return vn();HRoomServer*s=hr_room_handle(a[0]);Value o=vsobj();stput(o.u.st,"rooms",vi((long long)s->nrooms));stput(o.u.st,"revision",vi((long long)s->revision));stput(o.u.st,"max_rooms",vi((long long)s->maxrooms));stput(o.u.st,"max_players",vi((long long)s->maxplayers));return o;}
static Value game_room_server_destroy(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HRoomServer*s=hr_room_handle(a[0]);if(!s)return vb(0);hr_room_server_dtor(s);s->kind=HK_CLOSED;return vb(1);}

static void hr_mm_free(HMatchmaker*m){if(!m)return;for(size_t i=0;i<m->n;i++)free(m->q[i].id);free(m->q);m->q=NULL;m->n=m->cap=0;}
static void hr_mm_dtor(void*p){hr_mm_free((HMatchmaker*)p);}
static HMatchmaker*hr_mm_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_MATCHMAKER?(HMatchmaker*)v.u.handle:NULL;}
static Value game_matchmaker_create(VM*vm,int n,Value*a){(void)vm;if(n<1||n>3||a[0].t!=VINT)return vn();int target=(int)a[0].u.i,team=n>=2&&a[1].t==VINT?(int)a[1].u.i:1;double win=n==3&&isnum(a[2])?dn(a[2]):200.0;if(target<2||target>HR_MAX_MM_MATCH||team<1||team*2!=target||!isfinite(win)||win<0||win>100000)return vn();HMatchmaker*m=(HMatchmaker*)xmalloc_dtor(sizeof(*m),hr_mm_dtor);memset(m,0,sizeof(*m));m->kind=HK_MATCHMAKER;m->target=target;m->team_size=team;m->skill_window=win;m->next_ticket=1;return (Value){.t=VHANDLE,.u.handle=m};}
static int hr_mm_find(HMatchmaker*m,const char*id){for(size_t i=0;i<m->n;i++)if(!strcmp(m->q[i].id,id))return (int)i;return -1;}
static Value game_matchmaker_enqueue(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hr_mm_handle(a[0])||a[1].t!=VSTR||!isnum(a[2]))return vi(0);HMatchmaker*m=hr_mm_handle(a[0]);if(!hr_name_ok(a[1].u.s,HR_MAX_PLAYER_ID))return vi(0);double skill=dn(a[2]);if(!isfinite(skill))return vi(0);if(hr_mm_find(m,a[1].u.s)>=0||m->n>=HR_MAX_MM_PLAYERS)return vi(0);if(m->n==m->cap){size_t nc=m->cap?m->cap*2:32;if(nc>HR_MAX_MM_PLAYERS)nc=HR_MAX_MM_PLAYERS;HMMPlayer*nq=(HMMPlayer*)realloc(m->q,nc*sizeof(*nq));if(!nq)return vi(0);m->q=nq;m->cap=nc;}HMMPlayer*p=&m->q[m->n++];p->id=hr_strdup(a[1].u.s);if(!p->id){m->n--;return vi(0);}p->skill=skill;p->ticket=m->next_ticket++;p->enqueued_ms=hr_now_ms();return vi((long long)p->ticket);}
static Value game_matchmaker_remove(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hr_mm_handle(a[0])||a[1].t!=VSTR)return vb(0);HMatchmaker*m=hr_mm_handle(a[0]);int ix=hr_mm_find(m,a[1].u.s);if(ix<0)return vb(0);free(m->q[ix].id);for(size_t i=(size_t)ix+1;i<m->n;i++)m->q[i-1]=m->q[i];m->n--;return vb(1);}
static void hr_mm_pick(HMatchmaker*m,size_t*out){
    size_t anchor=0;uint64_t oldest=UINT64_MAX;for(size_t i=0;i<m->n;i++)if(m->q[i].enqueued_ms<oldest){oldest=m->q[i].enqueued_ms;anchor=i;}double base=m->q[anchor].skill;out[0]=anchor;for(int k=1;k<m->target;k++){size_t best=(size_t)-1;double score=DBL_MAX;for(size_t i=0;i<m->n;i++){int used=0;for(int j=0;j<k;j++)if(out[j]==i){used=1;break;}if(used)continue;double d=fabs(m->q[i].skill-base);if(d>m->skill_window&&best!=(size_t)-1)continue;double age=(double)(hr_now_ms()-m->q[i].enqueued_ms)/1000.0;double s=d-age*0.25;if(s<score){score=s;best=i;}}if(best==(size_t)-1){for(size_t i=0;i<m->n;i++){int used=0;for(int j=0;j<k;j++)if(out[j]==i){used=1;break;}if(!used){best=i;break;}}}out[k]=best;}
}
static Value game_matchmaker_tick(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hr_mm_handle(a[0]))return vn();HMatchmaker*m=hr_mm_handle(a[0]);Value matches=va();if(m->n<(size_t)m->target)return matches;size_t idx[HR_MAX_MM_MATCH];hr_mm_pick(m,idx);Value match=vsobj();Value teams=va(),t0=va(),t1=va();double sums[2]={0,0};for(int k=0;k<m->target;k++){HMMPlayer*p=&m->q[idx[k]];int team=(sums[0]<=sums[1])?0:1;Value z=vsobj();stput(z.u.st,"id",vs(p->id));stput(z.u.st,"skill",vf(p->skill));stput(z.u.st,"ticket",vi((long long)p->ticket));stput(z.u.st,"team",vi(team));ap(team?t1.u.a:t0.u.a,z);sums[team]+=p->skill;}ap(teams.u.a,t0);ap(teams.u.a,t1);stput(match.u.st,"teams",teams);stput(match.u.st,"size",vi(m->target));double smin=m->q[idx[0]].skill,smax=smin;for(int kk=1;kk<m->target;kk++){if(m->q[idx[kk]].skill<smin)smin=m->q[idx[kk]].skill;if(m->q[idx[kk]].skill>smax)smax=m->q[idx[kk]].skill;}stput(match.u.st,"skill_span",vf(smax-smin));ap(matches.u.a,match);m->formed++;/* remove in descending index order */for(int k=m->target-1;k>=0;k--){size_t x=idx[k];free(m->q[x].id);for(size_t j=x+1;j<m->n;j++)m->q[j-1]=m->q[j];m->n--;}return matches;}
static Value game_matchmaker_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hr_mm_handle(a[0]))return vn();HMatchmaker*m=hr_mm_handle(a[0]);Value o=vsobj();stput(o.u.st,"queued",vi((long long)m->n));stput(o.u.st,"target",vi(m->target));stput(o.u.st,"team_size",vi(m->team_size));stput(o.u.st,"skill_window",vf(m->skill_window));stput(o.u.st,"formed",vi((long long)m->formed));return o;}
static Value game_matchmaker_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HMatchmaker*m=hr_mm_handle(a[0]);if(!m)return vb(0);hr_mm_free(m);m->kind=HK_CLOSED;return vb(1);}

static void hr_rep_dtor(void*p){HReplication*r=(HReplication*)p;if(!r)return;free(r->last);r->last=NULL;r->last_n=r->cap=0;}
static HReplication*hr_rep_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_REPLICATION?(HReplication*)v.u.handle:NULL;}
static int hr_rep_reserve(HReplication*r,size_t n){if(n<=r->cap)return 1;size_t nc=r->cap?r->cap:1024;while(nc<n){if(nc>r->max_state/2){nc=r->max_state;break;}nc*=2;}if(nc<n)return 0;unsigned char*q=(unsigned char*)realloc(r->last,nc);if(!q)return 0;r->last=q;r->cap=nc;return 1;}
static size_t hr_rle_encode(const unsigned char*in,size_t n,unsigned char*out,size_t cap){size_t i=0,w=0;while(i<n){size_t run=1;while(i+run<n&&run<255&&in[i+run]==in[i])run++;if(run>=4){if(w+3>cap)return 0;out[w++]=1;out[w++]=(unsigned char)run;out[w++]=in[i];i+=run;continue;}size_t start=i,count=0;while(i<n&&count<255){run=1;while(i+run<n&&run<255&&in[i+run]==in[i])run++;if(run>=4)break;i++;count++;}if(!count){if(w+3>cap)return 0;out[w++]=1;out[w++]=(unsigned char)run;out[w++]=in[i];i+=run;}else{if(w+2+count>cap)return 0;out[w++]=0;out[w++]=(unsigned char)count;memcpy(out+w,in+start,count);w+=count;}}return w;}
static int hr_rle_decode(const unsigned char*in,size_t n,unsigned char*out,size_t raw){size_t i=0,w=0;while(i<n){if(n-i<2)return 0;unsigned op=in[i++],count=in[i++];if(op==0){if((size_t)count>n-i||w+(size_t)count>raw)return 0;memcpy(out+w,in+i,count);i+=count;w+=count;}else if(op==1){if(i>=n||w+(size_t)count>raw)return 0;memset(out+w,in[i++],count);w+=count;}else return 0;}return w==raw;}
static int hr_rep_encode(const unsigned char*raw,const unsigned char*base,size_t n,int delta,Value*packed){
    size_t cap=n*2+16;if(cap<n||cap>HR_MAX_STATE*2+16)return 0;unsigned char*tmp=(unsigned char*)malloc(n?n:1);unsigned char*enc=(unsigned char*)malloc(cap);if(!tmp||!enc){free(tmp);free(enc);return 0;}if(delta){for(size_t i=0;i<n;i++)tmp[i]=raw[i]^base[i];}else if(n)memcpy(tmp,raw,n);size_t wn=hr_rle_encode(tmp,n,enc,cap);if(wn==0&&n){free(tmp);free(enc);return 0;}Value arr=va();if(!arr_reserve(arr.u.a,wn+2)){free(tmp);free(enc);return 0;}ap(arr.u.a,vi(delta?1:0));ap(arr.u.a,vi((long long)n));for(size_t i=0;i<wn;i++)ap(arr.u.a,vi(enc[i]));free(tmp);free(enc);*packed=arr;return 1;
}
static int hr_rep_packed_bytes(Value v,unsigned char**out,size_t*n,int*delta,size_t*raw_n){if(v.t!=VARR||v.u.a->n<2)return 0;Value d=v.u.a->v[0],rn=v.u.a->v[1];if(d.t!=VINT||rn.t!=VINT||rn.u.i<0||rn.u.i>HR_MAX_STATE)return 0;size_t wn=v.u.a->n-2;unsigned char*b=(unsigned char*)malloc(wn?wn:1);if(!b)return 0;if(d.u.i<0||d.u.i>1){free(b);return 0;}for(size_t i=0;i<wn;i++){Value x=v.u.a->v[i+2];if(x.t!=VINT||x.u.i<0||x.u.i>255){free(b);return 0;}b[i]=(unsigned char)x.u.i;}*out=b;*n=wn;*delta=d.u.i!=0;*raw_n=(size_t)rn.u.i;return 1;}
static Value game_replication_create(VM*vm,int n,Value*a){(void)vm;if(n>1)return vn();size_t max=n==1&&a[0].t==VINT?(size_t)a[0].u.i:HR_MAX_STATE;if(max<1||max>HR_MAX_STATE)return vn();HReplication*r=(HReplication*)xmalloc_dtor(sizeof(*r),hr_rep_dtor);memset(r,0,sizeof(*r));r->kind=HK_REPLICATION;r->max_state=max;return (Value){.t=VHANDLE,.u.handle=r};}
static Value game_replication_snapshot(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!hr_rep_handle(a[0])||a[1].t!=VSTR)return vn();HReplication*r=hr_rep_handle(a[0]);size_t len=strlen(a[1].u.s);if(len>r->max_state)return vn();const unsigned char*raw=(const unsigned char*)a[1].u.s;int can_delta=r->last&&r->last_n==len;int force=n==3&&a[2].t==VBOOL&&a[2].u.b;Value packed=vn();int use_delta=can_delta&&!force;/* Build compressed candidates and choose the smaller representation. */Value full=vn(),del=vn();if(!hr_rep_encode(raw,r->last,len,0,&full))return vn();size_t full_sz=full.u.a->n;if(can_delta&&!hr_rep_encode(raw,r->last,len,1,&del))return vn();if(can_delta&&del.t==VARR&&del.u.a->n+1<full_sz)use_delta=1;else use_delta=0;packed=use_delta?del:full;if(!hr_rep_reserve(r,len?len:1))return vn();if(len)memcpy(r->last,raw,len);r->last_n=len;r->version++;r->snapshots++;if(use_delta)r->deltas++;else r->fulls++;Value o=vsobj();stput(o.u.st,"version",vi((long long)r->version));stput(o.u.st,"delta",vb(use_delta));stput(o.u.st,"raw_size",vi((long long)len));stput(o.u.st,"packed",packed);return o;}
static Value game_replication_apply(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hr_rep_handle(a[0])||a[1].t!=VSTRUCT)return vn();HReplication*r=hr_rep_handle(a[0]);Value pv=stget(a[1].u.st,"packed"),dv=stget(a[1].u.st,"delta"),rv=stget(a[1].u.st,"raw_size");if(dv.t!=VBOOL||rv.t!=VINT||rv.u.i<0||rv.u.i>HR_MAX_STATE)return vn();unsigned char*enc=NULL;size_t en=0,raw_n=0;int delta=0;if(!hr_rep_packed_bytes(pv,&enc,&en,&delta,&raw_n)){free(enc);return vn();}if((size_t)rv.u.i!=raw_n||dv.u.b!=delta){free(enc);return vn();}if(delta&&!r->last) {free(enc);return vn();}if(delta&&r->last_n!=raw_n){free(enc);return vn();}unsigned char*out=(unsigned char*)malloc(raw_n+1);if(!out){free(enc);return vn();}if(!hr_rle_decode(enc,en,out,raw_n)){free(enc);free(out);return vn();}if(delta)for(size_t i=0;i<raw_n;i++)out[i]^=r->last[i];free(enc);if(!hr_rep_reserve(r,raw_n?raw_n:1)){free(out);return vn();}if(raw_n)memcpy(r->last,out,raw_n);r->last_n=raw_n;r->version++;Value o=vsobj();stput(o.u.st,"version",vi((long long)r->version));stput(o.u.st,"delta",vb(delta));stput(o.u.st,"raw_size",vi((long long)raw_n));stput(o.u.st,"bytes",net_bytes_array(out,raw_n));if(raw_n&&raw_n<HR_MAX_STATE&&!memchr(out,0,raw_n)&&utf8_valid(out,raw_n)){char*z=(char*)malloc(raw_n+1);if(z){memcpy(z,out,raw_n);z[raw_n]=0;stput(o.u.st,"text",vs(z));free(z);}}free(out);return o;}
static Value game_replication_predict(VM*vm,int n,Value*a){if(n<3||n>4||!hr_rep_handle(a[0])||(a[1].t!=VSTR&&a[1].t!=VARR)||(a[2].t!=VSTR&&a[2].t!=VARR))return vn();size_t il=0;unsigned char*input=NULL;if(!hr_valid_bytes(a[2],&input,&il)||il>HR_MAX_PREDICT_INPUT){free(input);return vn();}if(n==4&&(a[3].t!=VFN&&a[3].t!=VNATIVE)){free(input);return vn();}Value args2[2]={a[1],a[2]};Value out=vn();if(n==4){if(a[3].t==VFN)out=run(vm,a[3].u.fn,2,args2);else out=a[3].u.native(vm,2,args2);}free(input);return out;}
static Value game_replication_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hr_rep_handle(a[0]))return vn();HReplication*r=hr_rep_handle(a[0]);Value o=vsobj();stput(o.u.st,"version",vi((long long)r->version));stput(o.u.st,"snapshots",vi((long long)r->snapshots));stput(o.u.st,"deltas",vi((long long)r->deltas));stput(o.u.st,"full",vi((long long)r->fulls));stput(o.u.st,"last_size",vi((long long)r->last_n));return o;}
static Value game_replication_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HReplication*r=hr_rep_handle(a[0]);if(!r)return vb(0);hr_rep_dtor(r);r->kind=HK_CLOSED;return vb(1);}


/* -------------------------------------------------------------------------
   v10.0 authoritative game-server runtime
   Fixed tick, prediction/reconciliation hooks, interpolation, lag compensation,
   reconnect lifecycle, bounded replay recording, match history and persistence.
   ------------------------------------------------------------------------- */
#define HR_GAME_MAX_PLAYERS 256u
#define HR_GAME_INPUTS 128u
#define HR_GAME_HISTORY 128u
#define HR_GAME_REPLAY_MAX_LINE 8192u
#define HR_GAME_MAX_MATCH_HISTORY 128u
#define HR_GAME_RECONNECT_MS 60000ULL
#define HR_GAME_IDLE_MS 15000ULL
#define HR_GAME_SLOT_TTL_MS 300000ULL

typedef struct HGameInput { uint64_t client_tick; uint32_t seq; double mx,my,mz; uint32_t buttons; uint64_t received_ms; int valid; } HGameInput;
typedef struct HGameFrame { uint64_t tick,time_ms; double x,y,z,vx,vy,vz; uint32_t input_seq; } HGameFrame;
typedef struct HGamePlayer {
    char *id,*token,*token_hash; double skill,mmr; int active,reconnecting,team,is_bot;
    uint64_t connected_ms,last_seen_ms,disconnect_ms; uint32_t last_applied_seq;
    double x,y,z,vx,vy,vz,hp; int32_t score; uint64_t games,wins,losses,last_match_id;
    HGameAI *bot; int bot_last_action,bot_have_state; double bot_state[8],bot_prev_hp; int32_t bot_prev_score;
    HGameInput inputs[HR_GAME_INPUTS]; size_t input_head,input_count;
    HGameFrame history[HR_GAME_HISTORY]; size_t hist_head,hist_count;
} HGamePlayer;
typedef struct HGameReplayFrame { uint64_t tick,time_ms; char *line; } HGameReplayFrame;
typedef struct HGameMatch { uint64_t id,start_ms,end_ms; char *winner; size_t players; } HGameMatch;
typedef struct HGameServer {
    int kind; unsigned tick_hz; uint64_t tick_ms,next_tick_ms,tick,created_ms;
    uint64_t tick_period_us,tick_period_floor_us,tick_period_rem_us,tick_period_rem_accum,next_tick_us;
    size_t max_players,nplayers; double move_speed; int closed;
    HGamePlayer players[HR_GAME_MAX_PLAYERS];
    HGameReplayFrame *replay; size_t replay_cap,replay_n,replay_head; int recording;
    HGameMatch matches[HR_GAME_MAX_MATCH_HISTORY]; size_t match_n,match_head; uint64_t next_match_id,current_match_id;
    char *spectators[64]; size_t spectator_n; char *admin_audit[64]; size_t admin_audit_n;
} HGameServer;

static HGameServer* game_server_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_GAME_SERVER?(HGameServer*)v.u.handle:NULL;}
static void gc_mark_game_server_handle(void*p){HGameServer*s=(HGameServer*)p;if(!s)return;for(size_t i=0;i<s->max_players&&i<HR_GAME_MAX_PLAYERS;i++)if(s->players[i].bot)gc_mark_handle(s->players[i].bot);}
static int game_server_player_index(HGameServer*s,const char*id){if(!s||!id)return -1;for(size_t i=0;i<s->max_players;i++)if(s->players[i].id&&!strcmp(s->players[i].id,id))return (int)i;return -1;}
static void game_server_player_reset(HGamePlayer*p){if(!p)return;free(p->id);free(p->token);free(p->token_hash);memset(p,0,sizeof(*p));}
static void game_server_match_free(HGameMatch*m){if(m)free(m->winner);}
static void game_server_replay_free(HGameReplayFrame*r){if(r)free(r->line);}
static void game_server_dtor(void*p){HGameServer*s=(HGameServer*)p;if(!s||s->closed)return;if(s->replay){for(size_t i=0;i<s->replay_cap;i++)game_server_replay_free(&s->replay[i]);free(s->replay);s->replay=NULL;s->replay_cap=s->replay_n=s->replay_head=0;}for(size_t i=0;i<s->max_players;i++)game_server_player_reset(&s->players[i]);for(size_t i=0;i<HR_GAME_MAX_MATCH_HISTORY;i++)game_server_match_free(&s->matches[i]);for(size_t i=0;i<s->spectator_n;i++)free(s->spectators[i]);for(size_t i=0;i<s->admin_audit_n;i++)free(s->admin_audit[i]);s->spectator_n=s->admin_audit_n=0;s->closed=1;s->nplayers=0;s->current_match_id=0;}
static char* game_server_token(void){unsigned char r[16];if(!random_bytes(r,sizeof r))return NULL;char*b=(char*)malloc(33);if(!b)return NULL;static const char*h="0123456789abcdef";for(size_t i=0;i<sizeof r;i++){b[i*2]=h[r[i]>>4];b[i*2+1]=h[r[i]&15];}b[32]=0;return b;}
static int game_server_record(HGameServer*s,uint64_t now_ms){
    if(!s||!s->recording||!s->replay||!s->replay_cap)return 1;
    size_t idx=s->replay_head%s->replay_cap; game_server_replay_free(&s->replay[idx]);
    char*line=(char*)malloc(HR_GAME_REPLAY_MAX_LINE); if(!line)return 0;
    int base=snprintf(line,HR_GAME_REPLAY_MAX_LINE,"tick=%llu,time=%llu",(unsigned long long)s->tick,(unsigned long long)now_ms);
    if(base<0){free(line);return 0;}
    size_t n=(size_t)base; if(n>=HR_GAME_REPLAY_MAX_LINE)n=HR_GAME_REPLAY_MAX_LINE-1;
    for(size_t i=0;i<s->max_players;i++){
        HGamePlayer*p=&s->players[i]; if(!p->id||!p->active)continue;
        if(n>=HR_GAME_REPLAY_MAX_LINE-1)break;
        int w=snprintf(line+n,HR_GAME_REPLAY_MAX_LINE-n,";p=%s,%d,%.6f,%.6f,%.6f,%.2f",p->id,p->team,p->x,p->y,p->z,p->hp);
        if(w<0)break; if((size_t)w>=HR_GAME_REPLAY_MAX_LINE-n){n=HR_GAME_REPLAY_MAX_LINE-1;break;} n+=(size_t)w;
    }
    line[n]=0; s->replay[idx]=(HGameReplayFrame){s->tick,now_ms,line};
    s->replay_head=(idx+1)%s->replay_cap; if(s->replay_n<s->replay_cap)s->replay_n++; return 1;
}
static void game_server_record_frame(HGamePlayer*p,uint64_t tick,uint64_t now){if(!p||!p->id)return;HGameFrame*f=&p->history[p->hist_head%HR_GAME_HISTORY];*f=(HGameFrame){tick,now,p->x,p->y,p->z,p->vx,p->vy,p->vz,p->last_applied_seq};p->hist_head=(p->hist_head+1)%HR_GAME_HISTORY;if(p->hist_count<HR_GAME_HISTORY)p->hist_count++;}
static HGameFrame game_server_frame_at(HGamePlayer*p,uint64_t t,int after){HGameFrame best={0};int have=0;uint64_t bestd=UINT64_MAX;for(size_t i=0;i<HR_GAME_HISTORY;i++){HGameFrame*f=&p->history[i];if(i>=p->hist_count||!f->time_ms)continue;if(after){if(f->time_ms<t)continue;if(!have||f->time_ms<best.time_ms)best=*f,have=1;}else{if(f->time_ms>t)continue;uint64_t d=t-f->time_ms;if(!have||d<bestd)best=*f,bestd=d,have=1;}}return best;}
static int game_server_enqueue_input(HGamePlayer*p,uint64_t ct,uint32_t seq,double mx,double my,double mz,uint32_t buttons,uint64_t now){if(!p||!seq||seq<=p->last_applied_seq)return 0;if(p->input_count>=HR_GAME_INPUTS){p->input_head=(p->input_head+1)%HR_GAME_INPUTS;p->input_count--;}size_t idx=(p->input_head+p->input_count)%HR_GAME_INPUTS;p->inputs[idx]=(HGameInput){ct,seq,mx,my,mz,buttons,now,1};p->input_count++;return 1;}
static HARIS_HOT HARIS_ALWAYS_INLINE void game_server_apply_inputs(HGameServer*s,HGamePlayer*p){HGameInput*best=NULL;for(size_t i=0;i<p->input_count;i++){HGameInput*x=&p->inputs[i];if(!x->valid||x->seq<=p->last_applied_seq)continue;if(!best||x->seq>best->seq)best=x;}if(!best)return;double mx=best->mx,my=best->my,mz=best->mz;double len2=mx*mx+my*my+mz*mz;if(!isfinite(len2)||len2>1000000.0){p->last_applied_seq=best->seq;return;}if(len2>1.0){double inv=1.0/sqrt(len2);mx*=inv;my*=inv;mz*=inv;}p->vx=mx*s->move_speed;p->vy=my*s->move_speed;p->vz=mz*s->move_speed;for(size_t i=0;i<p->input_count;i++)if(p->inputs[i].valid&&p->inputs[i].seq<=best->seq)p->inputs[i].valid=0;p->last_applied_seq=best->seq;}
static void game_server_bot_state(HGamePlayer*p,double*out){out[0]=p->x/1000.0;out[1]=p->y/1000.0;out[2]=p->z/1000.0;out[3]=p->vx/10.0;out[4]=p->vy/10.0;out[5]=p->vz/10.0;out[6]=p->hp/100.0;out[7]=(double)p->score/100.0;}
static Value game_server_bot_state_value(HGamePlayer*p){double x[8];game_server_bot_state(p,x);Value a=va();for(int i=0;i<8;i++)ap(a.u.a,vf(x[i]));return a;}
static HARIS_HOT void game_server_bot_think(VM*vm,HGameServer*s,HGamePlayer*p,uint64_t now){(void)vm;if(!p||!p->bot||!p->active)return;double state[8],prev[8];game_server_bot_state(p,state);if(p->bot_have_state){memcpy(prev,p->bot_state,(size_t)p->bot->in*sizeof(double));double reward=0.01+(double)(p->score-p->bot_prev_score)-fmax(0.0,p->bot_prev_hp-p->hp)*0.02;game_ai_remember_raw(p->bot,prev,p->bot_last_action,reward,state,0);}int act=game_ai_act_raw(p->bot,state);if(act<0)return;if(act>=p->bot->actions)act=0;double mx=0,my=0,mz=0;if(act==1)mx=1;else if(act==2)mx=-1;else if(act==3)my=1;else if(act==4)my=-1;uint32_t seq=p->last_applied_seq+1;if(seq==0)seq=1;game_server_enqueue_input(p,s->tick,seq,mx,my,mz,0,now);p->last_seen_ms=now;p->bot_last_action=act;p->bot_prev_hp=p->hp;p->bot_prev_score=p->score;memcpy(p->bot_state,state,(size_t)p->bot->in*sizeof(double));p->bot_have_state=1;if(s->tick%16==0)ai_game_train_step(vm,2,(Value[]){(Value){.t=VHANDLE,.u.handle=p->bot},vi(8)});if(s->tick%128==0)ai_game_target_update(vm,2,(Value[]){(Value){.t=VHANDLE,.u.handle=p->bot},vf(0.05)});}
static HARIS_HOT void game_server_step(VM*vm,HGameServer*s,uint64_t now){double dt=1.0/(double)s->tick_hz;s->tick++;for(size_t i=0;i<s->max_players;i++){HGamePlayer*p=&s->players[i];if(!p->id||!p->active)continue;if(p->is_bot)game_server_bot_think(vm,s,p,now);if(hr_elapsed_ms(now,p->last_seen_ms)>HR_GAME_RECONNECT_MS){if(p->is_bot){p->last_seen_ms=now;}else{p->active=0;p->reconnecting=1;p->disconnect_ms=now;if(now-p->last_seen_ms>HR_GAME_SLOT_TTL_MS){game_server_player_reset(p);if(s->nplayers)s->nplayers--;}}continue;}p->reconnecting=(hr_elapsed_ms(now,p->last_seen_ms)>HR_GAME_IDLE_MS)&&!p->is_bot;game_server_apply_inputs(s,p);p->x+=p->vx*dt;p->y+=p->vy*dt;p->z+=p->vz*dt;game_server_record_frame(p,s->tick,now);}game_server_record(s,now);}
static Value game_server_create(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"game.server_create");if(n>3)return vn();unsigned hz=n>=1&&a[0].t==VINT?(unsigned)a[0].u.i:30u;size_t maxp=n>=2&&a[1].t==VINT?(size_t)a[1].u.i:64u;size_t replay=n>=3&&a[2].t==VINT?(size_t)a[2].u.i:2048u;if(hz<5||hz>240||maxp<1||maxp>HR_GAME_MAX_PLAYERS||replay<1||replay>16384)return vn();HGameServer*s=(HGameServer*)xmalloc_dtor(sizeof(*s),game_server_dtor);memset(s,0,sizeof* s);s->kind=HK_GAME_SERVER;s->tick_hz=hz;s->tick_ms=1000u/hz;s->tick_period_us=1000000ULL/(uint64_t)hz;s->tick_period_floor_us=s->tick_period_us;s->tick_period_rem_us=1000000ULL%(uint64_t)hz;s->max_players=maxp;s->replay_cap=replay;s->recording=1;s->move_speed=6.0;s->created_ms=hr_now_ms();s->replay=(HGameReplayFrame*)calloc(replay,sizeof(*s->replay));if(!s->replay){game_server_dtor(s);return vn();}return (Value){.t=VHANDLE,.u.handle=s};}
static Value game_server_join(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!game_server_handle(a[0])||a[1].t!=VSTR)return vn();HGameServer*s=game_server_handle(a[0]);if(!hr_name_ok(a[1].u.s,HR_MAX_PLAYER_ID))return vn();int ix=game_server_player_index(s,a[1].u.s);uint64_t now=hr_now_ms();if(ix>=0&&n==3&&a[2].t==VSTR){HGamePlayer*p=&s->players[ix];int token_ok=(p->token&&strcmp(a[2].u.s,p->token)==0);if(!token_ok&&p->token_hash){char h[65]={0};token_ok=sha256_mem_hex((const unsigned char*)a[2].u.s,strlen(a[2].u.s),h)&&strcmp(h,p->token_hash)==0;}if(token_ok){p->active=1;p->reconnecting=0;p->last_seen_ms=now;return vb(1);}}if(ix>=0||s->nplayers>=s->max_players)return vn();for(size_t i=0;i<s->max_players;i++)if(!s->players[i].id){HGamePlayer*p=&s->players[i];memset(p,0,sizeof*p);p->id=hr_strdup(a[1].u.s);p->token=game_server_token();if(p->token){char h[65]={0};if(sha256_mem_hex((const unsigned char*)p->token,strlen(p->token),h))p->token_hash=hr_strdup(h);}if(!p->id||!p->token||!p->token_hash){game_server_player_reset(p);return vn();}p->active=1;p->last_seen_ms=now;p->connected_ms=now;p->hp=100.0;p->skill=1200.0;p->mmr=1200.0;s->nplayers++;Value o=vsobj();stput(o.u.st,"player",vs(p->id));stput(o.u.st,"reconnect_token",vs(p->token));stput(o.u.st,"team",vi((long long)p->team));stput(o.u.st,"tick",vi((long long)s->tick));return o;}return vn();}
static Value game_server_leave(VM*vm,int n,Value*a){(void)vm;if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR)return vb(0);HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vb(0);game_server_player_reset(&s->players[ix]);if(s->nplayers)s->nplayers--;return vb(1);}
static Value game_server_reconnect(VM*vm,int n,Value*a){(void)vm;if(n!=3||!game_server_handle(a[0])||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vb(0);HGamePlayer*p=&s->players[ix];int ok=(p->token&&strcmp(p->token,a[2].u.s)==0);if(!ok&&p->token_hash){char h[65]={0};ok=sha256_mem_hex((const unsigned char*)a[2].u.s,strlen(a[2].u.s),h)&&strcmp(h,p->token_hash)==0;}if(!ok)return vb(0);p->active=1;p->reconnecting=0;p->last_seen_ms=hr_now_ms();return vb(1);}
static Value game_server_input(VM*vm,int n,Value*a){(void)vm;if(n<7||n>8||!game_server_handle(a[0])||a[1].t!=VSTR||a[2].t!=VINT||a[3].t!=VINT||!isnum(a[4])||!isnum(a[5])||!isnum(a[6]))return vb(0);HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0||!s->players[ix].active)return vb(0);double mx=dn(a[4]),my=dn(a[5]),mz=dn(a[6]);if(!isfinite(mx)||!isfinite(my)||!isfinite(mz)||fabs(mx)>1000||fabs(my)>1000||fabs(mz)>1000)return vb(0);uint32_t buttons=n==8&&a[7].t==VINT?(uint32_t)a[7].u.i:0;uint64_t now=hr_now_ms();HGamePlayer*p=&s->players[ix];p->last_seen_ms=now;return vb(game_server_enqueue_input(p,(uint64_t)(a[2].u.i<0?0:a[2].u.i),(uint32_t)(a[3].u.i<0?0:a[3].u.i),mx,my,mz,buttons,now));}
static void game_server_mark_stale(HGameServer*s,uint64_t now){
    if(!s)return;
    for(size_t i=0;i<s->max_players;i++){
        HGamePlayer*p=&s->players[i];
        if(!p->id)continue;
        if(p->active && hr_elapsed_ms(now,p->last_seen_ms)>HR_GAME_RECONNECT_MS){
            p->active=0;p->reconnecting=1;p->disconnect_ms=now;p->vx=p->vy=p->vz=0;
        }
        if(p->id && p->reconnecting && hr_elapsed_ms(now,p->disconnect_ms)>HR_GAME_SLOT_TTL_MS){
            game_server_player_reset(p);
            if(s->nplayers)s->nplayers--;
        }
    }
}
static void game_server_tick_deadline_advance(HGameServer*s){if(!s)return;s->next_tick_us+=s->tick_period_floor_us;s->tick_period_rem_accum+=s->tick_period_rem_us;if(s->tick_period_rem_accum>=(uint64_t)s->tick_hz){s->next_tick_us+=s->tick_period_rem_accum/(uint64_t)s->tick_hz;s->tick_period_rem_accum%=(uint64_t)s->tick_hz;}s->next_tick_ms=s->next_tick_us/1000ULL;}
static Value game_server_tick(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||!game_server_handle(a[0]))return vi(0);HGameServer*s=game_server_handle(a[0]);uint64_t now_us=n==2&&a[1].t==VINT?(uint64_t)(a[1].u.i<0?0:a[1].u.i)*1000ULL:hr_now_us();uint64_t now_ms=now_us/1000ULL;game_server_mark_stale(s,now_ms);if(!s->next_tick_us)s->next_tick_us=now_us;unsigned steps=0;while(now_us>=s->next_tick_us&&steps<8){game_server_step(vm,s,s->next_tick_us/1000ULL);game_server_tick_deadline_advance(s);steps++;}return vi((long long)steps);}
static Value game_server_snapshot(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vn();HGameServer*s=game_server_handle(a[0]);Value out=va();for(size_t i=0;i<s->max_players;i++){HGamePlayer*p=&s->players[i];if(!p->id)continue;Value o=vsobj();stput(o.u.st,"player",vs(p->id));stput(o.u.st,"active",vb(p->active));stput(o.u.st,"reconnecting",vb(p->reconnecting));stput(o.u.st,"team",vi((long long)p->team));stput(o.u.st,"x",vf(p->x));stput(o.u.st,"y",vf(p->y));stput(o.u.st,"z",vf(p->z));stput(o.u.st,"vx",vf(p->vx));stput(o.u.st,"vy",vf(p->vy));stput(o.u.st,"vz",vf(p->vz));stput(o.u.st,"hp",vf(p->hp));stput(o.u.st,"score",vi((long long)p->score));stput(o.u.st,"input_seq",vi((long long)p->last_applied_seq));ap(out.u.a,o);}return out;}
static Value game_server_interpolate(VM*vm,int n,Value*a){(void)vm;if(n!=3||!game_server_handle(a[0])||a[1].t!=VSTR||a[2].t!=VINT)return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vn();HGamePlayer*p=&s->players[ix];uint64_t t=(uint64_t)(a[2].u.i<0?0:a[2].u.i);HGameFrame b=game_server_frame_at(p,t,0),q=game_server_frame_at(p,t,1);if(!b.time_ms&&!q.time_ms)return vn();if(!b.time_ms)b=q;if(!q.time_ms)q=b;double k=(q.time_ms>b.time_ms)?(double)(t-b.time_ms)/(double)(q.time_ms-b.time_ms):0;if(k<0)k=0;if(k>1)k=1;Value o=vsobj();stput(o.u.st,"time_ms",vi((long long)t));stput(o.u.st,"x",vf(b.x+(q.x-b.x)*k));stput(o.u.st,"y",vf(b.y+(q.y-b.y)*k));stput(o.u.st,"z",vf(b.z+(q.z-b.z)*k));stput(o.u.st,"alpha",vf(k));return o;}
static Value game_server_lag_compensate(VM*vm,int n,Value*a){(void)vm;if((n!=3&&n!=4)||!game_server_handle(a[0])||a[1].t!=VSTR||a[2].t!=VINT)return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vn();uint64_t rew=(uint64_t)(a[2].u.i<0?0:a[2].u.i);if(rew>2000)rew=2000;uint64_t now=(n==4&&a[3].t==VINT)?(uint64_t)(a[3].u.i<0?0:a[3].u.i):hr_now_ms();if(now<rew)return vn();HGameFrame f=game_server_frame_at(&s->players[ix],now-rew,0);if(!f.time_ms)return vn();Value o=vsobj();stput(o.u.st,"tick",vi((long long)f.tick));stput(o.u.st,"time_ms",vi((long long)f.time_ms));stput(o.u.st,"x",vf(f.x));stput(o.u.st,"y",vf(f.y));stput(o.u.st,"z",vf(f.z));return o;}
static Value game_server_reconcile(VM*vm,int n,Value*a){(void)vm;if(n<6||n>7||!game_server_handle(a[0])||a[1].t!=VSTR||a[2].t!=VINT||!isnum(a[3])||!isnum(a[4])||!isnum(a[5]))return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vn();double tol=n==7&&isnum(a[6])?dn(a[6]):0.25;if(!isfinite(tol)||tol<0||tol>1000)return vn();HGamePlayer*p=&s->players[ix];HGameFrame target={0};for(size_t i=0;i<p->hist_count;i++)if(p->history[i].tick==(uint64_t)(a[2].u.i<0?0:a[2].u.i)){target=p->history[i];break;}if(!target.time_ms)target=(HGameFrame){s->tick,hr_now_ms(),p->x,p->y,p->z,p->vx,p->vy,p->vz,p->last_applied_seq};double dx=target.x-dn(a[3]),dy=target.y-dn(a[4]),dz=target.z-dn(a[5]);Value o=vsobj();stput(o.u.st,"corrected",vb(fabs(dx)>tol||fabs(dy)>tol||fabs(dz)>tol));stput(o.u.st,"dx",vf(dx));stput(o.u.st,"dy",vf(dy));stput(o.u.st,"dz",vf(dz));stput(o.u.st,"x",vf(target.x));stput(o.u.st,"y",vf(target.y));stput(o.u.st,"z",vf(target.z));return o;}
static Value game_server_match_start(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vi(0);HGameServer*s=game_server_handle(a[0]);if(s->current_match_id)return vi((long long)s->current_match_id);s->current_match_id=++s->next_match_id;size_t idx=s->match_head%HR_GAME_MAX_MATCH_HISTORY;game_server_match_free(&s->matches[idx]);s->matches[idx]=(HGameMatch){s->current_match_id,hr_now_ms(),0,NULL,s->nplayers};for(size_t i=0;i<s->max_players;i++)if(s->players[i].id&&s->players[i].active)s->players[i].last_match_id=s->current_match_id;s->match_head=(idx+1)%HR_GAME_MAX_MATCH_HISTORY;if(s->match_n<HR_GAME_MAX_MATCH_HISTORY)s->match_n++;return vi((long long)s->current_match_id);}
static Value game_server_match_end(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||!game_server_handle(a[0]))return vb(0);HGameServer*s=game_server_handle(a[0]);if(!s->current_match_id)return vb(0);uint64_t id=s->current_match_id;const char*winner=(n==2&&a[1].t==VSTR)?a[1].u.s:NULL;size_t participants=0;for(size_t i=0;i<s->max_players;i++){HGamePlayer*p=&s->players[i];if(!p->id||p->last_match_id!=id)continue;participants++;}double avg=0;for(size_t i=0;i<s->max_players;i++){HGamePlayer*p=&s->players[i];if(p->id&&p->last_match_id==id)avg+=p->mmr;}if(participants)avg/=participants;for(size_t i=0;i<s->max_players;i++){HGamePlayer*p=&s->players[i];if(!p->id||p->last_match_id!=id)continue;double expected=1.0/(1.0+pow(10.0,(avg-p->mmr)/400.0));double result=(winner&&strcmp(p->id,winner)==0)?1.0:0.0;double delta=32.0*(result-expected);p->mmr=fmax(0.0,fmin(3000.0,p->mmr+delta));p->games++;if(result>0.5)p->wins++;else p->losses++;}
    size_t idx=(s->match_head+HR_GAME_MAX_MATCH_HISTORY-1)%HR_GAME_MAX_MATCH_HISTORY;for(size_t i=0;i<s->match_n;i++){size_t j=(idx+HR_GAME_MAX_MATCH_HISTORY-i)%HR_GAME_MAX_MATCH_HISTORY;if(s->matches[j].id==id){s->matches[j].end_ms=hr_now_ms();if(winner){free(s->matches[j].winner);s->matches[j].winner=hr_strdup(winner);}break;}}s->current_match_id=0;return vb(1);}
static Value game_server_match_history(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vn();HGameServer*s=game_server_handle(a[0]);Value out=va();size_t latest=(s->match_head+HR_GAME_MAX_MATCH_HISTORY-1)%HR_GAME_MAX_MATCH_HISTORY;for(size_t k=0;k<s->match_n;k++){size_t idx=(latest+HR_GAME_MAX_MATCH_HISTORY-k)%HR_GAME_MAX_MATCH_HISTORY;HGameMatch*m=&s->matches[idx];if(!m->id)continue;Value o=vsobj();stput(o.u.st,"id",vi((long long)m->id));stput(o.u.st,"start_ms",vi((long long)m->start_ms));stput(o.u.st,"end_ms",vi((long long)m->end_ms));stput(o.u.st,"players",vi((long long)m->players));stput(o.u.st,"winner",m->winner?vs(m->winner):vn());ap(out.u.a,o);}return out;}
static int game_server_add_spectator(HGameServer*s,const char*id){if(!s||!id)return 0;for(size_t i=0;i<s->spectator_n;i++)if(!strcmp(s->spectators[i],id))return 1;if(s->spectator_n>=64)return 0;s->spectators[s->spectator_n++]=hr_strdup(id);return s->spectators[s->spectator_n-1]!=NULL;}
static const char*game_server_rank_tier(double mmr){if(mmr>=2400)return "master";if(mmr>=2100)return "diamond";if(mmr>=1800)return "platinum";if(mmr>=1500)return "gold";if(mmr>=1200)return "silver";return "bronze";}
static Value game_server_add_bot(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!game_server_handle(a[0])||!game_ai_handle(a[1])||(n==3&&a[2].t!=VSTR))return vn();HGameServer*s=game_server_handle(a[0]);char idbuf[64];const char*want=n==3?a[2].u.s:NULL;if(want&&!hr_name_ok(want,HR_MAX_PLAYER_ID))return vn();for(size_t k=0;k<s->max_players;k++){if(s->players[k].id)continue;char*id=NULL;if(want)id=hr_strdup(want);else{for(unsigned q=1;q<10000;q++){snprintf(idbuf,sizeof idbuf,"bot%u",q);if(game_server_player_index(s,idbuf)<0){id=hr_strdup(idbuf);break;}}}if(!id)return vn();HGamePlayer*p=&s->players[k];memset(p,0,sizeof*p);p->id=id;p->token=game_server_token();if(p->token){char h[65]={0};if(sha256_mem_hex((const unsigned char*)p->token,strlen(p->token),h))p->token_hash=hr_strdup(h);}if(!p->token||!p->token_hash){game_server_player_reset(p);return vn();}p->active=1;p->is_bot=1;p->bot=(HGameAI*)a[1].u.handle;p->last_seen_ms=hr_now_ms();p->connected_ms=p->last_seen_ms;p->hp=100;p->skill=(double)p->bot->skill_rating;p->mmr=(double)p->bot->skill_rating;s->nplayers++;Value o=vsobj();stput(o.u.st,"player",vs(p->id));stput(o.u.st,"bot",vb(1));stput(o.u.st,"skill",vi(p->bot->skill_rating));stput(o.u.st,"team",vi((long long)p->team));return o;}return vn();}
static Value game_server_bot_info(VM*vm,int n,Value*a){(void)vm;if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR)return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0||!s->players[ix].is_bot||!s->players[ix].bot)return vn();HGamePlayer*p=&s->players[ix];Value o=vsobj();stput(o.u.st,"player",vs(p->id));stput(o.u.st,"skill",vi(p->bot->skill_rating));stput(o.u.st,"action",vi(p->bot_last_action));stput(o.u.st,"replay",vi((long long)p->bot->replay_n));stput(o.u.st,"learn_steps",vi((long long)p->bot->learn_steps));stput(o.u.st,"epsilon",vf(p->bot->epsilon));return o;}
static Value game_server_spectate(VM*vm,int n,Value*a){(void)vm;if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR)return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vn();if(!game_server_add_spectator(s,a[1].u.s))return vn();HGamePlayer*p=&s->players[ix];Value o=vsobj();stput(o.u.st,"spectating",vs(p->id));stput(o.u.st,"tick",vi((long long)s->tick));stput(o.u.st,"active",vb(p->active));stput(o.u.st,"x",vf(p->x));stput(o.u.st,"y",vf(p->y));stput(o.u.st,"z",vf(p->z));stput(o.u.st,"hp",vf(p->hp));stput(o.u.st,"score",vi((long long)p->score));stput(o.u.st,"mmr",vf(p->mmr));stput(o.u.st,"bot",vb(p->is_bot));stput(o.u.st,"snapshot",game_server_snapshot(vm,1,a));return o;}
static Value game_server_spectators(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vn();HGameServer*s=game_server_handle(a[0]);Value out=va();for(size_t i=0;i<s->spectator_n;i++)if(s->spectators[i])ap(out.u.a,vs(s->spectators[i]));return out;}
static Value game_server_rank(VM*vm,int n,Value*a){(void)vm;if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR)return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vn();HGamePlayer*p=&s->players[ix];double wr=p->games?(100.0*(double)p->wins/(double)p->games):0.0;Value o=vsobj();stput(o.u.st,"player",vs(p->id));stput(o.u.st,"mmr",vf(p->mmr));stput(o.u.st,"rank",vs(game_server_rank_tier(p->mmr)));stput(o.u.st,"games",vi((long long)p->games));stput(o.u.st,"wins",vi((long long)p->wins));stput(o.u.st,"losses",vi((long long)p->losses));stput(o.u.st,"win_rate",vf(wr));stput(o.u.st,"bot",vb(p->is_bot));return o;}
static Value game_server_leaderboard(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vn();HGameServer*s=game_server_handle(a[0]);int idxs[HR_GAME_MAX_PLAYERS];size_t nn=0;for(size_t i=0;i<s->max_players&&i<HR_GAME_MAX_PLAYERS;i++)if(s->players[i].id)idxs[nn++]=(int)i;for(size_t i=1;i<nn;i++){int key=idxs[i];size_t j=i;while(j>0){HGamePlayer*A=&s->players[idxs[j-1]],*B=&s->players[key];if(A->mmr>B->mmr+1e-12||(fabs(A->mmr-B->mmr)<=1e-12&&strcmp(A->id,B->id)<=0))break;idxs[j]=idxs[j-1];j--;}idxs[j]=key;}Value out=va();for(size_t i=0;i<nn;i++){HGamePlayer*p=&s->players[idxs[i]];double wr=p->games?(100.0*(double)p->wins/(double)p->games):0.0;Value o=vsobj();stput(o.u.st,"position",vi((long long)(i+1)));stput(o.u.st,"player",vs(p->id));stput(o.u.st,"mmr",vf(p->mmr));stput(o.u.st,"rank",vs(game_server_rank_tier(p->mmr)));stput(o.u.st,"wins",vi((long long)p->wins));stput(o.u.st,"losses",vi((long long)p->losses));stput(o.u.st,"win_rate",vf(wr));stput(o.u.st,"bot",vb(p->is_bot));ap(out.u.a,o);}return out;}
static Value game_server_replay_play(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"game.server.replay_play");if(n!=1||a[0].t!=VSTR||!fs_path_allowed(vm,a[0].u.s)||!safe_path_arg(a[0].u.s))return vn();char*src=readf_limit(a[0].u.s,32ULL*1024ULL*1024ULL);if(!src)return vn();char*save=NULL;char*line=strtok_r(src,"\n",&save);if(!line||strcmp(line,"HARIS_REPLAY_V1")){xfree(src);return vn();}line=strtok_r(NULL,"\n",&save);if(!line){xfree(src);return vn();}char*ep=NULL;long hz=strtol(line,&ep,10);if(!ep||*ep||hz<1||hz>240){xfree(src);return vn();}Value frames=va();size_t count=0;uint64_t first=0,last=0;for(line=strtok_r(NULL,"\n",&save);line&&count<100000;line=strtok_r(NULL,"\n",&save)){unsigned long long tick=0,tm=0;int used=0;if(sscanf(line,"tick=%llu,time=%llu%n",&tick,&tm,&used)!=2||used<=0)continue;Value fr=vsobj();stput(fr.u.st,"tick",vi((long long)tick));stput(fr.u.st,"time_ms",vi((long long)tm));Value pls=va();char*cur=line+used;while(*cur){while(*cur==';')cur++;if(strncmp(cur,"p=",2))break;cur+=2;char*next=strchr(cur,';');if(next)*next=0;char pid[128];int team=0;double x=0,y=0,z=0,hp=0;if(sscanf(cur,"%127[^,],%d,%lf,%lf,%lf,%lf",pid,&team,&x,&y,&z,&hp)==6&&hr_name_ok(pid,HR_MAX_PLAYER_ID)&&isfinite(x)&&isfinite(y)&&isfinite(z)&&isfinite(hp)){Value po=vsobj();stput(po.u.st,"player",vs(pid));stput(po.u.st,"team",vi(team));stput(po.u.st,"x",vf(x));stput(po.u.st,"y",vf(y));stput(po.u.st,"z",vf(z));stput(po.u.st,"hp",vf(hp));ap(pls.u.a,po);}if(!next)break;cur=next+1;}stput(fr.u.st,"players",pls);ap(frames.u.a,fr);if(count==0)first=(uint64_t)tick;last=(uint64_t)tick;count++;}xfree(src);Value o=vsobj();stput(o.u.st,"version",vi(1));stput(o.u.st,"tick_hz",vi(hz));stput(o.u.st,"frames",frames);stput(o.u.st,"frame_count",vi((long long)count));stput(o.u.st,"first_tick",vi((long long)first));stput(o.u.st,"last_tick",vi((long long)last));return o;}
static Value game_server_cmd(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GAME_ADMIN))return cap_error(vm,CAP_GAME_ADMIN,"game.server.cmd");if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR)return vn();HGameServer*s=game_server_handle(a[0]);char op[32]={0},target[HR_MAX_PLAYER_ID+1]={0},extra[2]={0};int fields=sscanf(a[1].u.s,"%31s %63s %1s",op,target,extra);Value o=vsobj();stput(o.u.st,"command",vs(a[1].u.s));if(!strcmp(op,"leaderboard")&&fields==1){stput(o.u.st,"ok",vb(1));stput(o.u.st,"result",game_server_leaderboard(vm,1,a));goto audit;}if(fields==2&&!strcmp(op,"kick")){int ix=game_server_player_index(s,target);if(ix<0){stput(o.u.st,"ok",vb(0));stput(o.u.st,"reason",vs("player_not_found"));goto audit;}if(!game_server_leave(vm,2,(Value[]){a[0],{.t=VSTR,.u.s=target}}).u.b){stput(o.u.st,"ok",vb(0));stput(o.u.st,"reason",vs("kick_failed"));goto audit;}stput(o.u.st,"ok",vb(1));stput(o.u.st,"player",vs(target));goto audit;}if(fields==2&&!strcmp(op,"spectate")){Value r=game_server_spectate(vm,2,(Value[]){a[0],{.t=VSTR,.u.s=target}});stput(o.u.st,"ok",vb(r.t!=VNULL));stput(o.u.st,"result",r);goto audit;}if(fields==2&&!strcmp(op,"rank")){Value r=game_server_rank(vm,2,(Value[]){a[0],{.t=VSTR,.u.s=target}});stput(o.u.st,"ok",vb(r.t!=VNULL));stput(o.u.st,"result",r);goto audit;}stput(o.u.st,"ok",vb(0));stput(o.u.st,"reason",vs("command_not_whitelisted"));
audit: if(s->admin_audit_n<64){s->admin_audit[s->admin_audit_n++]=hr_strdup(a[1].u.s);}else{free(s->admin_audit[0]);memmove(s->admin_audit,s->admin_audit+1,63*sizeof(char*));s->admin_audit[63]=hr_strdup(a[1].u.s);}return o;}
static Value game_server_admin_audit(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vn();HGameServer*s=game_server_handle(a[0]);Value out=va();for(size_t i=0;i<s->admin_audit_n;i++)if(s->admin_audit[i])ap(out.u.a,vs(s->admin_audit[i]));return out;}
static int game_server_write_state(HGameServer*s,const char*path){if(!s||!path||!safe_path_arg(path))return 0;char tmp[PATH_MAX];if(snprintf(tmp,sizeof tmp,"%s.tmp",path)<=0||strlen(tmp)>=sizeof tmp-1)return 0;FILE*f=fopen(tmp,"wb");if(!f)return 0;fprintf(f,"HARIS_GAME_STATE_V2\n%u %llu\n",s->tick_hz,(unsigned long long)s->tick);for(size_t i=0;i<s->max_players;i++){HGamePlayer*p=&s->players[i];if(!p->id)continue;char toksha[65]="";if(p->token_hash)snprintf(toksha,sizeof toksha,"%s",p->token_hash);else if(p->token)sha256_mem_hex((const unsigned char*)p->token,strlen(p->token),toksha);fprintf(f,"%s\t%s\t%d\t%d\t%d\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%lld\n",p->id,toksha,p->active,p->reconnecting,p->team,p->skill,p->x,p->y,p->z,p->vx,p->vy,p->vz,p->hp,(long long)p->score);}if(fflush(f)!=0){fclose(f);remove(tmp);return 0;}
#ifdef _WIN32
    if(_commit(_fileno(f))!=0){fclose(f);remove(tmp);return 0;}
#else
    if(fsync(fileno(f))!=0){fclose(f);remove(tmp);return 0;}
#endif
    if(fclose(f)!=0){remove(tmp);return 0;}
#ifdef _WIN32
    if(!MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){remove(tmp);return 0;}
    return 1;
#else
    return rename(tmp,path)==0;
#endif
}
static Value game_server_save(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"game.server_save");if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR||!fs_path_allowed(vm,a[1].u.s)||!safe_path_arg(a[1].u.s))return vb(0);return vb(game_server_write_state(game_server_handle(a[0]),a[1].u.s));}
static Value game_server_load(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"game.server_load");
    if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR||!fs_path_allowed(vm,a[1].u.s)||!safe_path_arg(a[1].u.s))return vb(0);
    HGameServer*s=game_server_handle(a[0]);if(s->nplayers)return vb(0);FILE*f=fopen(a[1].u.s,"rb");if(!f)return vb(0);
    char line[512];if(!fgets(line,sizeof line,f)||strncmp(line,"HARIS_GAME_STATE_V2",19)!=0){fclose(f);return vb(0);}if(!fgets(line,sizeof line,f)){fclose(f);return vb(0);}unsigned hz=0;unsigned long long tick=0;if(sscanf(line,"%u %llu",&hz,&tick)!=2||hz<5||hz>240){fclose(f);return vb(0);}if(hz!=s->tick_hz){fclose(f);return vb(0);}size_t loaded=0;uint64_t now=hr_now_ms();
    while(fgets(line,sizeof line,f)){
        char id[HR_MAX_PLAYER_ID+1]={0},hash[65]={0};int active=0,reconn=0,team=0;double skill=0,x=0,y=0,z=0,vx=0,vy=0,vz=0,hp=0;long long score=0;
        int got=sscanf(line,"%63s\t%64s\t%d\t%d\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lld",id,hash,&active,&reconn,&team,&skill,&x,&y,&z,&vx,&vy,&vz,&hp,&score);
        if(got!=14||!hr_name_ok(id,HR_MAX_PLAYER_ID)||strlen(hash)!=64||!isfinite(skill)||!isfinite(x)||!isfinite(y)||!isfinite(z)||!isfinite(vx)||!isfinite(vy)||!isfinite(vz)||!isfinite(hp)||hp<0||hp>1000000||team<-1||team>1024){fclose(f);for(size_t i=0;i<s->max_players;i++)if(s->players[i].id)game_server_player_reset(&s->players[i]);s->nplayers=0;return vb(0);}if(loaded>=s->max_players){fclose(f);for(size_t i=0;i<s->max_players;i++)if(s->players[i].id)game_server_player_reset(&s->players[i]);s->nplayers=0;return vb(0);}HGamePlayer*p=&s->players[loaded++];memset(p,0,sizeof*p);p->id=hr_strdup(id);p->token_hash=hr_strdup(hash);p->active=0;p->reconnecting=1;p->team=team;p->skill=skill;p->x=x;p->y=y;p->z=z;p->vx=vx;p->vy=vy;p->vz=vz;p->hp=hp;p->score=(int32_t)(score>INT32_MAX?INT32_MAX:score<INT32_MIN?INT32_MIN:score);p->connected_ms=now;p->last_seen_ms=now;p->disconnect_ms=now;if(!p->id||!p->token_hash){fclose(f);for(size_t i=0;i<s->max_players;i++)if(s->players[i].id)game_server_player_reset(&s->players[i]);s->nplayers=0;return vb(0);}}
    fclose(f);s->nplayers=loaded;s->tick=(uint64_t)tick;s->next_tick_ms=now;return vb(1);
}
static Value game_server_replay_save(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"game.server_replay_save");if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR||!fs_path_allowed(vm,a[1].u.s)||!safe_path_arg(a[1].u.s))return vb(0);HGameServer*s=game_server_handle(a[0]);FILE*f=fopen(a[1].u.s,"wb");if(!f)return vb(0);fprintf(f,"HARIS_REPLAY_V1\n%u\n",s->tick_hz);size_t start=s->replay_n==s->replay_cap?s->replay_head:0;for(size_t i=0;i<s->replay_n;i++){size_t idx=(start+i)%s->replay_cap;if(s->replay[idx].line)fprintf(f,"%s\n",s->replay[idx].line);}return vb(fclose(f)==0);}
static Value game_server_replay_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vn();HGameServer*s=game_server_handle(a[0]);Value o=vsobj();stput(o.u.st,"recording",vb(s->recording));stput(o.u.st,"frames",vi((long long)s->replay_n));stput(o.u.st,"capacity",vi((long long)s->replay_cap));return o;}
static Value game_server_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_server_handle(a[0]))return vn();HGameServer*s=game_server_handle(a[0]);size_t reconnect=0;for(size_t i=0;i<s->max_players;i++)if(s->players[i].id&&s->players[i].reconnecting)reconnect++;Value o=vsobj();stput(o.u.st,"tick",vi((long long)s->tick));stput(o.u.st,"tick_hz",vi((long long)s->tick_hz));stput(o.u.st,"tick_ms",vi((long long)s->tick_ms));stput(o.u.st,"players",vi((long long)s->nplayers));stput(o.u.st,"reconnecting",vi((long long)reconnect));stput(o.u.st,"current_match",vi((long long)s->current_match_id));uint64_t now=hr_now_ms();stput(o.u.st,"uptime_ms",vi((long long)hr_elapsed_ms(now,s->created_ms)));return o;}
static Value game_server_player_state(VM*vm,int n,Value*a){
    (void)vm;if(n!=8||!game_server_handle(a[0])||a[1].t!=VSTR||!isnum(a[2])||!isnum(a[3])||!isnum(a[4])||!isnum(a[5])||a[6].t!=VINT||a[7].t!=VINT)return vb(0);
    HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vb(0);HGamePlayer*p=&s->players[ix];
    double x=dn(a[2]),y=dn(a[3]),z=dn(a[4]),hp=dn(a[5]);if(!isfinite(x)||!isfinite(y)||!isfinite(z)||!isfinite(hp)||hp<0||hp>1000000)return vb(0);
    long long score=a[6].u.i;long long team=a[7].u.i;if(team<-1||team>1024)return vb(0);
    p->x=x;p->y=y;p->z=z;p->hp=hp;p->score=(int32_t)(score>INT32_MAX?INT32_MAX:score<INT32_MIN?INT32_MIN:score);p->team=(int)team;p->last_seen_ms=hr_now_ms();
    return vb(1);
}
static Value game_server_player_info(VM*vm,int n,Value*a){
    (void)vm;if(n!=2||!game_server_handle(a[0])||a[1].t!=VSTR)return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vn();HGamePlayer*p=&s->players[ix];
    Value o=vsobj();stput(o.u.st,"player",vs(p->id));stput(o.u.st,"active",vb(p->active));stput(o.u.st,"reconnecting",vb(p->reconnecting));stput(o.u.st,"team",vi((long long)p->team));stput(o.u.st,"x",vf(p->x));stput(o.u.st,"y",vf(p->y));stput(o.u.st,"z",vf(p->z));stput(o.u.st,"hp",vf(p->hp));stput(o.u.st,"score",vi((long long)p->score));stput(o.u.st,"input_seq",vi((long long)p->last_applied_seq));return o;
}
static Value game_server_damage(VM*vm,int n,Value*a){
    (void)vm;if(n<3||n>4||!game_server_handle(a[0])||a[1].t!=VSTR||!isnum(a[2]))return vn();HGameServer*s=game_server_handle(a[0]);int ix=game_server_player_index(s,a[1].u.s);if(ix<0)return vn();double dmg=dn(a[2]);if(!isfinite(dmg)||dmg<=0||dmg>1000000)return vn();HGamePlayer*p=&s->players[ix];if(!p->active)return vb(0);double old=p->hp;p->hp=fmax(0.0,p->hp-dmg);int killed=(old>0&&p->hp<=0);if(killed&&n==4&&a[3].t==VSTR){int ax=game_server_player_index(s,a[3].u.s);if(ax>=0&&ax!=ix)s->players[ax].score++;}Value o=vsobj();stput(o.u.st,"ok",vb(1));stput(o.u.st,"damage",vf(old-p->hp));stput(o.u.st,"hp",vf(p->hp));stput(o.u.st,"killed",vb(killed));stput(o.u.st,"target",vs(p->id));return o;
}
static Value game_server_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HGameServer*s=game_server_handle(a[0]);if(!s)return vb(0);if(s->closed)return vb(1);game_server_dtor(s);s->kind=HK_CLOSED;return vb(1);}
static Value os_sleep_ms(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_OS))return cap_error(vm,CAP_OS,"os.sleep_ms");if(n!=1||a[0].t!=VINT)return vb(0);long long ms=a[0].u.i;if(ms<0)ms=0;if(ms>600000)ms=600000;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts={(time_t)(ms/1000),(long)(ms%1000)*1000000L};while(nanosleep(&ts,&ts)!=0&&errno==EINTR){}
#endif
    return vb(1);}
static Value os_monotonic_ms(VM*vm,int n,Value*a){(void)a;if(!cap_allowed(vm,CAP_OS))return cap_error(vm,CAP_OS,"os.monotonic_ms");if(n!=0)return vi(0);return vi((long long)hr_now_ms());}
static Value nint_cast(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();if(a[0].t==VINT)return a[0];if(a[0].t==VBOOL)return vi(a[0].u.b?1:0);if(a[0].t==VFLOAT){if(!isfinite(a[0].u.f)||a[0].u.f>(double)LLONG_MAX||a[0].u.f<(double)LLONG_MIN)return vn();return vi((long long)a[0].u.f);}if(a[0].t==VSTR){char*e=NULL;errno=0;long long v=strtoll(a[0].u.s,&e,10);if(errno||!e||*e)return vn();return vi(v);}return vn();}
static Value nfloat_cast(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();if(a[0].t==VFLOAT)return a[0];if(a[0].t==VINT)return vf((double)a[0].u.i);if(a[0].t==VBOOL)return vf(a[0].u.b?1.0:0.0);if(a[0].t==VSTR){char*e=NULL;errno=0;double v=strtod(a[0].u.s,&e);if(errno||!e||*e||!isfinite(v))return vn();return vf(v);}return vn();}
static Value game_server_set_speed(VM*vm,int n,Value*a){(void)vm;if(n!=2||!game_server_handle(a[0])||!isnum(a[1]))return vb(0);double x=dn(a[1]);if(!isfinite(x)||x<=0||x>1000)return vb(0);game_server_handle(a[0])->move_speed=x;return vb(1);}
static void bind_v10_server(VM*vm){bindn(&vm->g,"game.server_create",game_server_create);bindn(&vm->g,"game.server.create",game_server_create);bindn(&vm->g,"game.server.join",game_server_join);bindn(&vm->g,"game.server.leave",game_server_leave);bindn(&vm->g,"game.server.reconnect",game_server_reconnect);bindn(&vm->g,"game.server.tick",game_server_tick);bindn(&vm->g,"game.server_join",game_server_join);bindn(&vm->g,"game.server_leave",game_server_leave);bindn(&vm->g,"game.server_reconnect",game_server_reconnect);bindn(&vm->g,"game.server_input",game_server_input);bindn(&vm->g,"game.server_tick",game_server_tick);bindn(&vm->g,"game.server_snapshot",game_server_snapshot);bindn(&vm->g,"game.server_interpolate",game_server_interpolate);bindn(&vm->g,"game.server_lag_compensate",game_server_lag_compensate);bindn(&vm->g,"game.server_reconcile",game_server_reconcile);bindn(&vm->g,"game.server_match_start",game_server_match_start);bindn(&vm->g,"game.server_match_end",game_server_match_end);bindn(&vm->g,"game.server_match_history",game_server_match_history);bindn(&vm->g,"game.server_save",game_server_save);bindn(&vm->g,"game.server_load",game_server_load);bindn(&vm->g,"game.server_replay_save",game_server_replay_save);bindn(&vm->g,"game.server.replay_save",game_server_replay_save);bindn(&vm->g,"game.server_replay_play",game_server_replay_play);bindn(&vm->g,"game.server.replay_play",game_server_replay_play);bindn(&vm->g,"game.server_replay_info",game_server_replay_info);bindn(&vm->g,"game.server.replay_info",game_server_replay_info);bindn(&vm->g,"game.server_add_bot",game_server_add_bot);bindn(&vm->g,"game.server.add_bot",game_server_add_bot);bindn(&vm->g,"game.server_bot_info",game_server_bot_info);bindn(&vm->g,"game.server.bot_info",game_server_bot_info);bindn(&vm->g,"game.server_spectate",game_server_spectate);bindn(&vm->g,"game.server.spectate",game_server_spectate);bindn(&vm->g,"game.server_spectators",game_server_spectators);bindn(&vm->g,"game.server.spectators",game_server_spectators);bindn(&vm->g,"game.server_rank",game_server_rank);bindn(&vm->g,"game.server.rank",game_server_rank);bindn(&vm->g,"game.server_leaderboard",game_server_leaderboard);bindn(&vm->g,"game.server.leaderboard",game_server_leaderboard);bindn(&vm->g,"game.server_cmd",game_server_cmd);bindn(&vm->g,"game.server.cmd",game_server_cmd);bindn(&vm->g,"game.server_admin_audit",game_server_admin_audit);bindn(&vm->g,"game.server.admin_audit",game_server_admin_audit);bindn(&vm->g,"game.server_info",game_server_info);bindn(&vm->g,"game.server_player_state",game_server_player_state);bindn(&vm->g,"game.server_player_info",game_server_player_info);bindn(&vm->g,"game.server_damage",game_server_damage);bindn(&vm->g,"game.server_set_speed",game_server_set_speed);bindn(&vm->g,"game.server_close",game_server_close);bindn(&vm->g,"os.sleep_ms",os_sleep_ms);}

static void hr_ac_dtor(void*p){HAntiCheat*a=(HAntiCheat*)p;if(!a)return;for(size_t i=0;i<a->n;i++)free(a->v[i].player);free(a->v);a->v=NULL;a->n=a->cap=0;}
static HAntiCheat*hr_ac_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_ANTICHEAT?(HAntiCheat*)v.u.handle:NULL;}
static HAcTrack*hr_ac_find(HAntiCheat*a,const char*id,int create){for(size_t i=0;i<a->n;i++)if(!strcmp(a->v[i].player,id))return &a->v[i];if(!create||a->n>=HR_AC_MAX_TRACKS)return NULL;if(a->n==a->cap){size_t nc=a->cap?a->cap*2:32;if(nc>HR_AC_MAX_TRACKS)nc=HR_AC_MAX_TRACKS;HAcTrack*n=(HAcTrack*)realloc(a->v,nc*sizeof(*n));if(!n)return NULL;a->v=n;a->cap=nc;}HAcTrack*t=&a->v[a->n++];memset(t,0,sizeof(*t));t->player=hr_strdup(id);t->tokens=a->burst;t->last_ms=hr_now_ms();return t;}
static Value game_anticheat_create(VM*vm,int n,Value*a){(void)vm;if(n>4)return vn();double rate=n>=1&&isnum(a[0])?dn(a[0]):HR_AC_DEFAULT_RATE;double burst=n>=2&&isnum(a[1])?dn(a[1]):HR_AC_MAX_BURST;double speed=n>=3&&isnum(a[2])?dn(a[2]):50.0;double tol=n>=4&&isnum(a[3])?dn(a[3]):1.0;if(!isfinite(rate)||!isfinite(burst)||!isfinite(speed)||!isfinite(tol)||rate<=0||burst<1||burst>10000||speed<=0||speed>HR_AC_MAX_SPEED||tol<0||tol>1000)return vn();HAntiCheat*c=(HAntiCheat*)xmalloc_dtor(sizeof(*c),hr_ac_dtor);memset(c,0,sizeof(*c));c->kind=HK_ANTICHEAT;c->rate=rate;c->burst=burst;c->max_speed=speed;c->tolerance=tol;return (Value){.t=VHANDLE,.u.handle=c};}
static Value game_anticheat_rate_limit(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!hr_ac_handle(a[0])||a[1].t!=VSTR)return vb(0);HAntiCheat*c=hr_ac_handle(a[0]);HAcTrack*t=hr_ac_find(c,a[1].u.s,1);if(!t)return vb(0);double cost=n==3&&isnum(a[2])?dn(a[2]):1.0;if(!isfinite(cost)||cost<=0||cost>c->burst)return vb(0);uint64_t now=hr_now_ms();double dt=(double)hr_elapsed_ms(now,t->last_ms)/1000.0;if(dt>0){t->tokens=fmin(c->burst,t->tokens+dt*c->rate);t->last_ms=now;}if(t->tokens+1e-9<cost)return vb(0);t->tokens-=cost;return vb(1);}
static Value game_anticheat_validate_move(VM*vm,int n,Value*a){(void)vm;if(n<6||n>7||!hr_ac_handle(a[0])||a[1].t!=VSTR||!isnum(a[2])||!isnum(a[3])||!isnum(a[4])||!isnum(a[5]))return vn();HAntiCheat*c=hr_ac_handle(a[0]);double x=dn(a[2]),y=dn(a[3]),z=dn(a[4]),dt=dn(a[5]);uint64_t seq=n==7&&a[6].t==VINT?(uint64_t)(a[6].u.i<0?0:a[6].u.i):0;if(!isfinite(x)||!isfinite(y)||!isfinite(z)||!isfinite(dt)||dt<=0||dt>HR_AC_MAX_DT)return vb(0);HAcTrack*t=hr_ac_find(c,a[1].u.s,1);if(!t)return vb(0);if(seq&&seq<=t->last_seq)return vb(0);if(!t->have_pos){t->x=x;t->y=y;t->z=z;t->have_pos=1;t->last_seq=seq;return vb(1);}double dx=x-t->x,dy=y-t->y,dz=z-t->z;double d=sqrt(dx*dx+dy*dy+dz*dz),maxd=c->max_speed*dt+c->tolerance;int ok=isfinite(d)&&d<=maxd;t->last_seq=seq;if(ok){t->x=x;t->y=y;t->z=z;t->warned=0;}else t->warned=1;return vb(ok);}
static Value game_anticheat_authoritative(VM*vm,int n,Value*a){
    (void)vm;if(n!=8&&n!=9)return vb(0);if(!hr_ac_handle(a[0])||a[1].t!=VSTR||!isnum(a[2])||!isnum(a[3])||!isnum(a[4])||!isnum(a[5])||!isnum(a[6])||!isnum(a[7]))return vb(0);
    HAntiCheat*c=hr_ac_handle(a[0]);double sx=dn(a[2]),sy=dn(a[3]),sz=dn(a[4]),cx=dn(a[5]),cy=dn(a[6]),cz=dn(a[7]);double tol=n==9&&isnum(a[8])?dn(a[8]):c->tolerance;
    if(!isfinite(sx)||!isfinite(sy)||!isfinite(sz)||!isfinite(cx)||!isfinite(cy)||!isfinite(cz)||!isfinite(tol)||tol<0)return vb(0);HAcTrack*t=hr_ac_find(c,a[1].u.s,1);if(!t)return vb(0);double dx=fabs(cx-sx),dy=fabs(cy-sy),dz=fabs(cz-sz);int ok=dx<=tol&&dy<=tol&&dz<=tol;t->x=sx;t->y=sy;t->z=sz;t->have_pos=1;t->warned=!ok;return vb(ok);
}
static Value game_anticheat_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hr_ac_handle(a[0]))return vn();HAntiCheat*c=hr_ac_handle(a[0]);Value o=vsobj();stput(o.u.st,"players",vi((long long)c->n));stput(o.u.st,"rate",vf(c->rate));stput(o.u.st,"burst",vf(c->burst));stput(o.u.st,"max_speed",vf(c->max_speed));stput(o.u.st,"tolerance",vf(c->tolerance));return o;}
static Value game_anticheat_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HAntiCheat*c=hr_ac_handle(a[0]);if(!c)return vb(0);hr_ac_dtor(c);c->kind=HK_CLOSED;return vb(1);}

/* Bindings are intentionally flat dotted names to match Haris' existing
   namespace convention. */
static void bind_v91_multiplayer(VM*vm){
    bindn(&vm->g,"net.reliable.open",net_reliable_open);bindn(&vm->g,"net.reliable.send",net_reliable_send);bindn(&vm->g,"net.reliable.poll",net_reliable_poll);bindn(&vm->g,"net.reliable.tick",net_reliable_tick);bindn(&vm->g,"net.reliable.info",net_reliable_info);bindn(&vm->g,"net.reliable.close",net_reliable_close);
    bindn(&vm->g,"game.room_server_create",game_room_server_create);bindn(&vm->g,"game.room_create",game_room_create);bindn(&vm->g,"game.room_join",game_room_join);bindn(&vm->g,"game.room_leave",game_room_leave);bindn(&vm->g,"game.room_players",game_room_players);bindn(&vm->g,"game.room_state_set",game_room_state_set);bindn(&vm->g,"game.room_state_get",game_room_state_get);bindn(&vm->g,"game.room_state_sync",game_room_state_sync);bindn(&vm->g,"game.room_server_info",game_room_server_info);bindn(&vm->g,"game.room_server_destroy",game_room_server_destroy);
    bindn(&vm->g,"game.matchmaker_create",game_matchmaker_create);bindn(&vm->g,"game.matchmaker_enqueue",game_matchmaker_enqueue);bindn(&vm->g,"game.matchmaker_remove",game_matchmaker_remove);bindn(&vm->g,"game.matchmaker_tick",game_matchmaker_tick);bindn(&vm->g,"game.matchmaker_info",game_matchmaker_info);bindn(&vm->g,"game.matchmaker_close",game_matchmaker_close);
    bindn(&vm->g,"game.replication_create",game_replication_create);bindn(&vm->g,"game.replication_snapshot",game_replication_snapshot);bindn(&vm->g,"game.replication_apply",game_replication_apply);bindn(&vm->g,"game.replication_predict",game_replication_predict);bindn(&vm->g,"game.replication_info",game_replication_info);bindn(&vm->g,"game.replication_close",game_replication_close);
    bindn(&vm->g,"game.anticheat_create",game_anticheat_create);bindn(&vm->g,"game.anticheat_rate_limit",game_anticheat_rate_limit);bindn(&vm->g,"game.anticheat_validate_move",game_anticheat_validate_move);bindn(&vm->g,"game.anticheat_authoritative",game_anticheat_authoritative);bindn(&vm->g,"game.anticheat_info",game_anticheat_info);bindn(&vm->g,"game.anticheat_close",game_anticheat_close);
}

#endif /* HARIS_V91_NETSTACK */



