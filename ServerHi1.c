



#include <stdio.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <pthread.h>

#include <errno.h>
#include <netinet/in.h>
#include <time.h>



#include<arpa/inet.h>



pthread_mutex_t  mutex;

struct packets{
    int syn;
    char payload[1000];
//    pthread_mutex_t  mutex;
    int lastop;
    unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
    unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
    __be16		h_proto;
    char ip_source[20];
    char ip_dest[20];
    int tcp_source;
    int tcp_dest;
    int seq;
    int ack_seq;

    int ack;
    int fin;
    int rst;
    int window;
    int nop;
    struct ethhdr *eth;
    struct iphdr *ip;
    struct tcphdr *tcp;
};// *p=NULL;

struct struct_flag{
    int syn;
    int ack;
    int fin;
    int seq;
    int ack_seq;
};
        struct struct_connection{
            char source_ip[20];
            char dest_ip[20];
            struct struct_flag tcp_flag[3];
            int hand_shake_state;
            int my_port;
            int dest_port;
            void(*get_response_packet)(char *packet);
            int is_sent;//toggles between 1 for sent in sending thread 0 for not in check_and_process_connection


        };
struct struct_super_struct{
    struct struct_connection **conn;
    struct packets *p;
    int _connection_num;

};
void allocate_super_con(struct struct_connection **conn);
int check_and_process_connection(struct struct_super_struct *super,struct packets *p);
#ifndef likely
# define likely(x)          __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
# define unlikely(x)                __builtin_expect(!!(x), 0)
#endif
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
struct block_desc {
    uint32_t version;
    uint32_t offset_to_priv;
    struct tpacket_hdr_v1 h1;
};

struct ring {
    struct iovec *rd;
    uint8_t *map;
    struct tpacket_req3 req;
};


void get_payload_to_send(struct packets *p,char **pay);
int setup_socket(struct ring *ring, char *netdev);
void sighandler(int num);
int display(struct tpacket3_hdr *ppd,struct packets *p1);
int walk_block(struct block_desc *pbd, const int block_num, struct packets **p );
void flush_block(struct block_desc *pbd);
void teardown_socket(struct ring *ring, int fd);
void * receiver(void *args);
void main();

volatile int nop=0;
void flush_block(struct block_desc *pbd)
{
    pbd->h1.block_status = TP_STATUS_KERNEL;
}
int setup_socket(struct ring *ring, char *netdev)
{
    int err, i, fd, v = TPACKET_V3;
    struct sockaddr_ll ll;
    unsigned int blocksiz = 1 << 22, framesiz = 1 << 11;
    unsigned int blocknum = 64;

    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        perror("socket");
        exit(1);
    }

    err = setsockopt(fd, SOL_PACKET, PACKET_VERSION, &v, sizeof(v));
    if (err < 0) {
        perror("setsockopt");
        exit(1);
    }

    memset(&ring->req, 0, sizeof(ring->req));
    ring->req.tp_block_size = blocksiz;
    ring->req.tp_frame_size = framesiz;
    ring->req.tp_block_nr = blocknum;
    ring->req.tp_frame_nr = (blocksiz * blocknum) / framesiz;
    ring->req.tp_retire_blk_tov = 60;
    ring->req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;

    err = setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &ring->req,
                     sizeof(ring->req));

    if (err < 0) {
        perror("setsockopt");
        exit(1);
    }

    ring->map = mmap(NULL, ring->req.tp_block_size * ring->req.tp_block_nr,
                     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
    if (ring->map == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    ring->rd = malloc(ring->req.tp_block_nr * sizeof(*ring->rd));
    assert(ring->rd);
    for (i = 0; i < ring->req.tp_block_nr; ++i) {
        ring->rd[i].iov_base = ring->map + (i * ring->req.tp_block_size);
        ring->rd[i].iov_len = ring->req.tp_block_size;
    }

    memset(&ll, 0, sizeof(ll));
    ll.sll_family = PF_PACKET;
    ll.sll_protocol = htons(ETH_P_ALL);
    ll.sll_ifindex = if_nametoindex(netdev);
    ll.sll_hatype = 0;
    ll.sll_pkttype = 0;
    ll.sll_halen = 0;

    err = bind(fd, (struct sockaddr *) &ll, sizeof(ll));
    if (err < 0) {
        perror("bind");
        exit(1);
    }

    return fd;
}

int initialize_mutexes(struct packets **p1)
{
    pthread_mutexattr_t mtxAttr;
    int i=0;
    int s=pthread_mutexattr_init(&mtxAttr);
    if(s!=0)
    {  printf("Errno: %d inside initialize_mutexes at pthread_mutextattr_init\n",errno);}
    s= pthread_mutexattr_settype(&mtxAttr,PTHREAD_MUTEX_ERRORCHECK);
    if(s!=0)
        printf("Errno: %d inside initialize_mutexes at pthread_mutexattr_settype\n",errno);


        s= pthread_mutex_init(&mutex,&mtxAttr);
        if(s!=0)
            printf("Errno: %d while(i++<5) element:%d inside initialize_mutexes at pthread_mutex_init\n",errno,i);



}
static sig_atomic_t sigint = 0;
void sighandler(int num)
{
    sigint = 1;
}
static unsigned long packets_total = 0, bytes_total = 0;

int display(struct tpacket3_hdr *ppd,struct packets *p)
{
   // printf("display\n");
    struct ethhdr *eth = (struct ethhdr *) ((uint8_t *) ppd + ppd->tp_mac);
    memcpy((p)->h_source,eth->h_source,sizeof(eth->h_source));
    memcpy((p)->h_dest,eth->h_dest,sizeof(eth->h_dest));
    (p)->eth=eth;
    //memcpy((p)->h_proto,eth->h_proto,sizeof(eth->h_proto));
    eth->h_proto=(p)->h_proto;
    struct iphdr *ip = (struct iphdr *) ((uint8_t *) eth + ETH_HLEN);
    (p)->ip=ip;

    struct tcphdr *tcp=(struct tcphdr *)((uint8_t *)ip+sizeof(struct iphdr));
    (p)->tcp=tcp;
   // if (ntohl(eth->h_proto) == ETH_P_IP)
    {
        struct sockaddr_in ss, sd;
        char sbuff[NI_MAXHOST], dbuff[NI_MAXHOST];

        memset(&ss, 0, sizeof(ss));
        ss.sin_family = PF_INET;
        ss.sin_addr.s_addr = ip->saddr;
        getnameinfo((struct sockaddr *) &ss, sizeof(ss),
                    sbuff, sizeof(sbuff), NULL, 0, NI_NUMERICHOST);

        memset(&sd, 0, sizeof(sd));
        sd.sin_family = PF_INET;
        sd.sin_addr.s_addr = ip->daddr;
        getnameinfo((struct sockaddr *) &sd, sizeof(sd),
                    dbuff, sizeof(dbuff), NULL, 0, NI_NUMERICHOST);

        strcpy((p)->ip_source,sbuff);
        strcpy((p)->ip_dest,dbuff);
        (p)->tcp_source=ntohs(tcp->source);
        (p)->tcp_dest=ntohs(tcp->dest);
        (p)->syn=tcp->syn;
        (p)->ack=tcp->ack;
        (p)->seq=ntohs(tcp->seq);
        (p)->ack_seq=ntohs(tcp->ack_seq);
        printf("lastop IN Receiver %d\n",(p)->lastop);
        (p)->lastop=1;

       // printf("%s -> %s, ", sbuff, dbuff);
    }

    printf("rxhash: 0x%x\n", ppd->hv1.tp_rxhash);
    return 1;
}

/*
int display(struct tpacket3_hdr *ppd,struct packets *p1)
{

    struct ethhdr *eth = (struct ethhdr *) (ppd + ppd->tp_mac);

    struct iphdr *ip = (struct iphdr *) ( eth + ETH_HLEN);
    struct tcphdr *tcp=(struct tcphdr *)(ip+sizeof(struct iphdr));
    //char *payload_body=(char *)(tcp + tcp->doff) ;
    char payload_body[2000];
    //memcpy(payload_body,0,sizeof(payload_body));
    
    memcpy(payload_body,((tcp + tcp->doff)),sizeof(char)*(1000-1)) ;
   (p1)->syn=1;

    int i=0;
    int j=0;
    int f=0;

 strcpy((p1)->payload,payload_body);
    (p1)->syn = tcp->syn;
    (p1)->ack = tcp->ack;
    (p1)->seq = tcp->seq;
    (p1)->ack_seq = tcp->ack_seq;
    (p1)->window = tcp->window;
    (p1)->rst = tcp->rst;
    (p1)->fin = tcp->fin;
    (p1)->lastop=1;
    struct sockaddr_in source;
    source.sin_addr.s_addr=ip->saddr;

    struct sockaddr_in dest;
    dest.sin_addr.s_addr=ip->daddr;
    getnameinfo((struct sockaddr *) &source, sizeof(source),
                (p1)->ip_source, sizeof(char)*20, NULL, 0, NI_NUMERICHOST);
    getnameinfo((struct sockaddr *) &dest, sizeof(dest),
                (p1)->ip_dest, sizeof(char)*20, NULL, 0, NI_NUMERICHOST);
    //(p1)->ip_source=(char *)malloc(sizeof(char)*20);
   // (p1)->ip_dest=(char *)malloc(sizeof(char)*20);

    strcpy((p1)->ip_source,inet_ntoa(source.sin_addr));
    strcpy((p1)->ip_dest,inet_ntoa(dest.sin_addr));

    printf("source: %s\n",(p1)->ip_source);
    printf("dest: %s\n",(p1)->ip_dest);


    (p1)->tcp_source=ntohs(tcp->source);
    (p1)->tcp_dest=ntohs(tcp->dest);

    //strcpy((p1)->ip_source,source1);
    //strcpy((p1)->ip_dest,dest1);

   */
/* if (eth->h_proto == htons(ETH_P_IP)) {
        struct sockaddr_in ss, sd;
        char sbuff[NI_MAXHOST], dbuff[NI_MAXHOST];
        memset(&ss, 0, sizeof(ss));
        ss.sin_family = PF_INET;
        ss.sin_addr.s_addr = ip->saddr;
        getnameinfo((struct sockaddr *) &ss, sizeof(ss),
                    sbuff, sizeof(sbuff), NULL, 0, NI_NUMERICHOST);
        memset(&sd, 0, sizeof(sd));
        sd.sin_family = PF_INET;
        sd.sin_addr.s_addr = ip->daddr;
        getnameinfo((struct sockaddr *) &sd, sizeof(sd),
                    dbuff, sizeof(dbuff), NULL, 0, NI_NUMERICHOST);
        char *cc=inet_ntoa(ss.sin_addr);

        int i=0;
        char *temp;
        if(ntohl(tcp->source)==443)
        {

        }
    }*//*



    return 1;
}
*/

int walk_block(struct block_desc *pbd, const int block_num,struct packets **p)
{
  // printf("walking\n");
    int num_pkts = pbd->h1.num_pkts, i;
    unsigned long bytes = 0;
    struct tpacket3_hdr *ppd;
    ppd = (struct tpacket3_hdr *) ((uint8_t *) pbd +
                                   pbd->h1.offset_to_first_pkt);
    
    // *p=(struct packets *) malloc(sizeof(struct packets)*num_pkts);
    nop=num_pkts;
    int ret=0;
    int h;
    if(num_pkts<20);
    else
    num_pkts=20;
    for (i = 0; i < num_pkts; ++i) {
        bytes += ppd->tp_snaplen;
        struct packets t;
    
       if(display(ppd,(*p+i))==1){

            h++;
        }
        ppd = (struct tpacket3_hdr *) ((uint8_t *) ppd +
                                     ppd->tp_next_offset);
    }
    ret=h;
    packets_total += num_pkts;
    bytes_total += bytes;
    return ret;
}

void * task_receiver(void *args)
{
    printf("receiver\n");
    struct struct_super_struct *super=(struct struct_super_struct *)args;

    struct packets *p=super->p; //(struct packets *)args;
    char *c=(char *)args;
    int fd, err;
    socklen_t len;
    struct ring ring;
    struct pollfd pfd;
    unsigned int block_num = 0, blocks = 64;
    struct block_desc *pbd;
    struct tpacket_stats_v3 stats;


    signal(SIGINT, sighandler);

    memset(&ring, 0, sizeof(ring));
    fd = setup_socket(&ring, "wlan0");
    assert(fd > 0);

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    pfd.events = POLLIN ;
    pfd.revents = 0;

  //  printf("started receiver \n");
    int o=0;
   // p=(struct packets *) malloc(sizeof(struct packets)*20);
    while (likely(!sigint))
    {

        pbd = (struct block_desc *) ring.rd[block_num].iov_base;
        if ((pbd->h1.block_status & TP_STATUS_USER) == 0) {
            poll(&pfd, 1, -1);
            continue;
        }
        int ret;
        o++;
       
        pthread_mutex_lock(&mutex);
       // printf("received block\n");
        ret=walk_block(pbd, block_num,&p);
        pthread_mutex_unlock(&mutex);
       // printf("sending signal to SENDER\n");
       // int s = pthread_cond_signal(&cond);
        if(nop>=2)
        {
            // pthread_cond_wait(&cond, &mutex);
            int s = pthread_cond_signal(&cond);
            pthread_cond_wait(&cond, &mutex);
        }
       /* Wake sleeping consumer */


        flush_block(pbd);
        block_num = (block_num + 1) % blocks;
        sleep(1);


    }

}
void * task_sender(void *args){
    struct struct_super_struct *super=(struct struct_super_struct *)args;

    struct packets *p=super->p;//(struct packets *)args;
    printf("Sender\n");
    while(1)
    {
        //printf("got uppder loop SENDER\n");


       pthread_mutex_lock(&mutex);
       
        while(nop==0)
        {

            pthread_cond_wait(&cond, &mutex);
          //  printf("finished waiting in SENDER\n");


        }
        int i=0;

        printf("NOP is greater than 2 in SENDER %d\n");

        while(nop>0)
         {
           //  check_and_process_connection(super,(p+i));


           //if(p==NULL)break;
       if(strcmp("192.168.10.25",(p+i)->ip_dest)==0 && (p+i)->syn==1 && (p+i)->ack==0 )
          {
              //sleep(1);

            //  printf("%s\n",(p+i)->ip_dest);
             // if(strcmp((p+i)->ip_dest,"192.168.10.25")==0 )
              {
                  char *pac;
                  struct packets *pt=(p+i);
                  get_payload_to_send((p+i),&pac);
                  struct sockaddr_in temp;
                  struct ethhdr *eth=(struct ethhdr *)pac;
                  struct iphdr *ip=(struct iphdr *)(pac+sizeof(struct ethhdr));
                  struct tcphdr *tcp=(struct tcphdr *)(pac+sizeof(struct ethhdr )+sizeof(struct iphdr));
                  temp.sin_addr.s_addr=ip->saddr;
                  char *source=inet_ntoa(temp.sin_addr);
                  struct sockaddr_in temp1;
                  temp1.sin_addr.s_addr=ip->daddr;
                  char *dest=inet_ntoa(temp1.sin_addr);
                  printf("%s >> %s \n",(p+i)->ip_source,(p+i)->ip_dest);
                 // if(strcmp("192.168.10.25",dest)==0 && strcmp("192.168.10.25",source)==0)
                  {
                      ///temp1.sin_addr.s_addr=ip->daddr;
                      printf("should be\n");
                      printf("frm %s to %s syc:%d ack:%d \n", inet_ntoa(temp.sin_addr), inet_ntoa(temp1.sin_addr),
                             tcp->syn, tcp->ack);
                  }
     /*          printf("__________________________________________________\n");
                  printf("lastop: %d\n",(p+i)->lastop);
               printf("source: %s %d\n",(p+i)->ip_source,(p+i)->tcp_source);
               printf("dest: %s %d\n",(p+i)->ip_dest,(p+i)->tcp_dest);
               printf("syc: %d\n",(p+i)->syn);
               printf("ack: %d\n",(p+i)->ack);
               printf("seq: %d\n",(p+i)->seq);
               printf("ack seq: %d\n",(p+i)->ack_seq);
               (p+i)->lastop=0;
               printf("lastop: %d\n",(p+i)->lastop);*/
              }
               // printf("dest port: %d\n",(p+i)->tcp_dest);
          }
       //else{ printf("source ip: %s\n",(p+i)->ip_source);printf("dest ip: %s\n",(p+i)->ip_dest);}
             //p++;
            nop--;
            i++;
         //   printf("just processed packet SENDER\n");
         }
       //p=NULL;


        pthread_mutex_unlock(&mutex);
        nop=0;
        int s = pthread_cond_signal(&cond);
    }
}
void main()
{
    pthread_t thread1,thread2;
    pthread_attr_t attr;
    struct packets *p;

    struct struct_super_struct *super=(struct struct_super_struct *)malloc(sizeof(struct struct_super_struct));
    super->_connection_num=0;
    //
    super->p=&p;
    super->p=(struct packets *) malloc(sizeof(struct packets)*40);
    super->conn=NULL;
    int s= pthread_attr_init(&attr);
    super->conn=NULL;
    if(s!=0){
        printf("Errno: %d  pthread_attr_init\n ",errno);
    }
    s=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    if(s!=0)
        printf("Errno: %d pthread_attr_setdetachstate\n",errno);
    initialize_mutexes(NULL);
    s= pthread_create(&thread1,&attr,task_receiver,(void *)super);
    if(s!=0)
    {
        printf("Errno: %d pthread_create -- receiver \n", errno);
        return;
    }

    s= pthread_create(&thread2,&attr,task_sender,(void *)super);
    if(s!=0)
    printf("Errno: %d pthread_create --sender \n",errno);




    s= pthread_attr_destroy(&attr);
    if(s!=0)
        printf("Errno: %d pthread_attr_destroy \n",errno);
    while(1);

    return;
}
/*
int check_and_process_connection(struct struct_super_struct *super,struct packets *p)
{
    if(p->syn==1 && p->ack==0 && p->tcp_dest==80 && strcmp(p->ip_dest,"192.168.10.25")==0)
    {

        if(super->conn==NULL){
            printf("super->conn is NULL\n");

        }
        struct struct_connection **conn=super->conn;
        allocate_super_con(super->conn);

        if(super->conn==NULL)
        {
            printf("super->con still NULL\n");
        }
        else
        {
            printf("super->conn is allocated\n");
        }
        return 0;
        printf("__________________________________________________\n");
        printf("lastop: %d\n",p->lastop);
        printf("source: %s %d\n",p->ip_source,p->tcp_source);
        printf("dest: %s %d\n",p->ip_dest,p->tcp_dest);
        printf("syc: %d\n",p->syn);
        printf("ack: %d\n",p->ack);
        printf("seq: %d\n",p->seq);
        printf("ack seq: %d\n",p->ack_seq);
        p->lastop=0;
        printf("lastop: %d\n",p->lastop);
        super->_connection_num++;

    }       //do it


}*/


void allocate_super_con(struct struct_connection **conn)
{
    if(conn==NULL)
    *conn=(struct struct_connection *)malloc(sizeof(struct struct_connection)*nop);

}
unsigned short csum(unsigned short *ptr,int nbytes)
{
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }

    sum = (sum>>16)+(sum & 0xffff);
    sum = sum + (sum>>16);
    answer=(short)~sum;

    return(answer);
}


void get_payload_to_send(struct packets *p,char **pay)
{
    printf("%s --->>> %s",p->ip_source,p->ip_dest);
    int t1;
    *pay=(char *) malloc(sizeof(t1)+1020/*1000=payload*/);
    memset (*pay, 0, sizeof(t1)+1020);
    struct ethhdr *eth = (struct ethhdr *) *pay;
    struct iphdr *ip = (struct iphdr *) (*pay + sizeof(struct ethhdr));
    struct tcphdr *tcp = (struct tcphdr *) (*pay + sizeof(struct ethhdr) + sizeof(struct iphdr));

 //   if(p->h_proto==ntohs(p->h_proto)==ETH_P_IP)
    {
        if(p->syn==1 && p->ack==0 && p->fin==0)
        {
            ///Ethernet
            //memcpy(eth->h_dest,p->h_source,sizeof(eth->h_source));

            eth->h_proto=htonl(ETH_P_IP);
            //memcpy(eth->h_source,p->h_dest,sizeof(eth->h_dest));
            ////IP

           // memcpy(ip->saddr,inet_addr(p->ip_dest),sizeof(ip->saddr));
           struct sockaddr_in s1,s2;
            s1.sin_family = AF_INET;
            s1.sin_port = htons(80);
            s1.sin_addr.s_addr=inet_addr(p->ip_source);


            s2.sin_family = AF_INET;
            s2.sin_port = htons(5009);
            s2.sin_addr.s_addr=inet_addr(p->ip_dest);
            //ip->saddr=s1.sin_addr.s_addr;
            sleep(2);
            ip->daddr=s2.sin_addr.s_addr;
            char c[20];
            memcpy(ip->saddr,inet_addr(p->ip_dest),sizeof(c)-1);
            ip->ihl=5;
            ip->version=4;
            ip->tos=0;
            ip->tot_len=sizeof(t1)+1020;
            srand(1001);
            ip->id=htonl(rand());
            ip->frag_off=0;
            ip->ttl=225;
            ip->protocol=IPPROTO_TCP;
            ip->check=0;
            tcp->syn=1;
            tcp->ack=1;



            memcpy(ip->daddr,inet_addr(p->ip_source),sizeof(c)-1);

            // ip->check=csum((unsigned short *)*pay,ip->tot_len);


        }
        else if(p->syn==0 && p->ack==1)
        {

        }
    }
    //printf("%saddr %s -->>",p->ip_source);
    //printf("daddr %s\n",p->ip_dest);
    //populate eth

}