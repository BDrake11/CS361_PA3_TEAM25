//---------------------------------------------------------------------
// Assignment : PA-03 UDP Single-Threaded Server
// Date       : 11/21/25
// Author     : Braden Drake, Aiden Smith
// File Name  : procurement.c
//---------------------------------------------------------------------

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "wrappers.h"
#include "message.h"

#define MAXFACTORIES    20

typedef struct sockaddr SA ;

/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    int     numFactories ,      // Total Number of Factory Threads
            activeFactories ,   // How many are still alive and manufacturing parts
            iters[ MAXFACTORIES+1 ] = {0} ,  // num Iterations completed by each Factory
            partsMade[ MAXFACTORIES+1 ] = {0} , totalItems = 0;

    char  *myName = "Braden Drake, Aiden Smith" ; 
    printf("\nPROCUREMENT: Started. Developed by %s\n\n" , myName );    

    char myUserName[30] ;
    getlogin_r ( myUserName , 30 ) ;
    time_t  now;
    time( &now ) ;
    fprintf( stdout , "Logged in as user '%s' on %s\n\n" , myUserName ,  ctime( &now)  ) ;
    fflush( stdout ) ;
    
    if ( argc < 4 )
    {
        printf("PROCUREMENT Usage: %s  <order_size> <FactoryServerIP>  <port>\n" , argv[0] );
        exit( -1 ) ;  
    }

    unsigned        orderSize  = atoi( argv[1] ) ;
    char	       *serverIP   = argv[2] ;
    unsigned short  port       = (unsigned short) atoi( argv[3] ) ;
 
    printf("Attempting Factory server at '%s' : %d\n", serverIP, port);

    /* Set up local and remote sockets */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
        err_sys("Could not create socket.");


    // Prepare the server's socket address structure
    struct sockaddr_in srvrSkt;

    // Clear the srvrSkt and 
    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIP, (void *) &srvrSkt.sin_addr.s_addr) != 1)
        err_sys("Invalid server IP address");

    
    // Send the initial request to the Factory Server
    msgBuf  msg1;
    memset(&msg1, 0, sizeof(msg1));

    msg1.purpose = htonl(REQUEST_MSG);
    msg1.orderSize = htonl(orderSize);
    sendto(sd, &msg1, sizeof(msg1), 0, (SA *) &srvrSkt, sizeof(srvrSkt));

    printf("\nPROCUREMENT Sent this message to the FACTORY server: "  );
    printMsg( & msg1 );
    puts("");


    /* Now, wait for order confirmation from the Factory server */
    msgBuf  msg2;
    memset(&msg2, 0, sizeof(msg2));
    unsigned int alen = sizeof(srvrSkt);

    printf ("\nPROCUREMENT is now waiting for order confirmation ...\n" );

    if (recvfrom(sd, &msg2, sizeof(msg2), 0, (SA *) &srvrSkt, &alen) < 0)
        err_sys("Error during recvfrom()");
    
    printf("PROCUREMENT received this from the FACTORY server: "  );
    printMsg( & msg2 ); 
    puts("\n");

    numFactories = ntohl(msg2.numFac);
    activeFactories = numFactories;

    msgBuf incomingMessage;
    unsigned int srvrLen = sizeof(srvrSkt);
    // Monitor all Active Factory Lines & Collect Production Reports
    while ( activeFactories > 0 ) // wait for messages from sub-factories
    {
        // Inspect the incoming message
        memset(&incomingMessage, 0, sizeof(incomingMessage));

        if (recvfrom(sd, &incomingMessage, sizeof(incomingMessage), 0, (SA *) &srvrSkt, &srvrLen) < 0)
            err_sys("Error during recvfrom()");

        int purpose = ntohl(incomingMessage.purpose);
        int facID = ntohl(incomingMessage.facID);

        if (purpose == PRODUCTION_MSG) {
            int parts = ntohl(incomingMessage.partsMade);
            int duration = ntohl(incomingMessage.duration);

            printf("PROCUREMENT: Factory #%-2d  produced %-5d parts in %-4d  milliSecs\n", facID, parts, duration);

            iters[facID]++;
            partsMade[facID] += parts;
        } else if (purpose == COMPLETION_MSG) {
            printf("PROCUREMENT: Factory #%-2d        COMPLETED its task\n\n", facID);
            activeFactories--;
        } else {
            incomingMessage.purpose = htonl(PROTOCOL_ERR);
            printf("PROCUREMENT: Received invalid msg ");
            printMsg(&incomingMessage);
            puts("\n");
            
            close(sd);
            exit(1);
        }       
    } 

    // Print the summary report
    totalItems  = 0 ;
    printf("\n\n****** PROCUREMENT Summary Report ******\n");

    for (int i = 1; i <= numFactories; i++) {
        totalItems += partsMade[i];

        printf("Factory # %2d made a total of %5d parts in %5d iterations\n", i, partsMade[i], iters[i]);
    }

    printf("==============================\n") ;

    printf("Grand total parts made = %5d   vs  order size of %5d\n", totalItems, orderSize);

    printf( "\n>>> PROCURMENT Terminated\n");

    close(sd);
    return 0 ;
}
