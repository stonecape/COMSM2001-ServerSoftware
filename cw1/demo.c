/*
 * This is a skeleton program for COMSM2001 (Server Software) coursework 1
 * "the project marking problem". Your task is to synchronise the threads
 * correctly by adding code in the places indicated by comments in the
 * student, marker and run functions.
 * You may create your own global variables and further functions.
 * The code in this skeleton program can be used without citation in the files
 * that you submit for your coursework.
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

/*
 * Parameters of the program. The constraints are D < T and
 * S*K <= M*N.
 */
#define MARKER_MAX_NUMBER 100
#define STUDENT_MAX_NUMBER 100

struct demo_parameters {
    int S;   /* Number of students */
    int M;   /* Number of markers */
    int K;   /* Number of markers per demo */
    int N;   /* Number of demos per marker */
    int T;   /* Length of session (minutes) */
    int D;   /* Length of demo (minutes) */
};

/* Global object holding the demo parameters. */
struct demo_parameters parameters;

/* The demo start time, set in the main function. Do not modify this. */
struct timeval starttime;

/*
 * You may wish to place some global variables here.
 * Remember, globals are shared between threads.
 * You can also create functions of your own.
 */
static pthread_mutex_t lock;// MUTEX
pthread_cond_t condition;// CONDITION
pthread_cond_t demoCond;// CONDITION for DEMO
pthread_cond_t markerReady;// CONDITION: all markers have been in the lab
pthread_cond_t timeoutCond;


int markerState[MARKER_MAX_NUMBER];// 0-idle; 1-assessing; index-markerID
int markerProjectCount[MARKER_MAX_NUMBER]; // count of assessed projects for each marker
int markerAssessStudentID[MARKER_MAX_NUMBER];// store studentId that the marker need to estimate; -1-not be grabbed
int demoState[STUDENT_MAX_NUMBER];// 0-not be assessed; 1-be assessed; index-studentID
int studentMarkerGrabCount[STUDENT_MAX_NUMBER];// count of grabbed marker

int notFullMarkerCount;
int idleMarkerCount;
int markerCount;// the number of markers who have entered lab
//int unassessedStudentCount;


void lockGrab() {
	int err = pthread_mutex_lock(&lock);
	if (err) { abort(); }

}

void unlockGrab() {
	int err = pthread_mutex_unlock(&lock);
	if (err) { abort(); }
}

/*
 * timenow(): returns current simulated time in "minutes" (cs).
 * Assumes that starttime has been set already.
 * This function is safe to call in the student and marker threads as
 * starttime is set in the run() function.
 */
int timenow() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - starttime.tv_sec) * 100 + (now.tv_usec - starttime.tv_usec) / 10000;
}

/*If time is out, it will return 1*/
int isTimeOut() {
	if(timenow() >= (parameters.T - parameters.D))
		return 1;
	return 0;
}

/* delay(t): delays for t "minutes" (cs) */
void delay(int t) {
    struct timespec rqtp, rmtp;
    t *= 10;
    rqtp.tv_sec = t / 1000;
    rqtp.tv_nsec = 1000000 * (t % 1000);
    nanosleep(&rqtp, &rmtp);
}

/* panic(): simulates a student's panicking activity */
void panic() {
    delay(random() % (parameters.T - parameters.D));
}

/* demo(): simulates a demo activity */
void demo() {
    delay(parameters.D);
}

/*
 * A marker thread. You need to modify this function.
 * The parameter arg is the number of the current marker and the function
 * doesn't need to return any values.
 * Do not modify the printed output as it will be used as part of the testing.
 */
void *marker(void *arg) {
    int markerID = *(int *)arg;

    /*
     * The following variable is used in the printf statements when a marker is
     * grabbed by a student. It shall be set by this function whenever the
     * marker is grabbed - and before the printf statements referencing it are
     * executed.
     */

    int studentID;

    /* 1. Enter the lab. */
    printf("%d marker %d: enters lab\n", timenow(), markerID);
	lockGrab();
	idleMarkerCount++;
	notFullMarkerCount++;
	markerCount++;
	pthread_cond_broadcast(&markerReady);
	unlockGrab();

	 /* A marker marks up to N projects. */
	/* 2. Repeat (N times).
	 *    (a) Wait to be grabbed by a student.
	 *    (b) Wait for the student's demo to begin
	 *        (you may not need to do anything here).
	 *    (c) Wait for the demo to finish.
	 *        Do not just wait a given time -
	 *        let the student signal when the demo is over.
	 *    (d) Exit the lab.
	 */

	while(markerProjectCount[markerID] < parameters.N) {

		/*
		 * 3. If the end of the session approaches (i.e. there is no time
		 *    to start another demo) then the marker waits for the current
		 *    demo to finish (if they are currently attending one) and then
		 *    exits the lab.
		 */
		//(a)
		lockGrab();
		//printf("--before while markerID=%d, markerState[markerID]=%d, markerAssessStudentID[markerID] =%d\n",markerID,markerState[markerID],markerAssessStudentID[markerID]);
		while(markerState[markerID] == 0 || markerAssessStudentID[markerID] == -1) {
			//printf("--after while markerID=%d, markerState[markerID]=%d, markerAssessStudentID[markerID] =%d\n",markerID,markerState[markerID],markerAssessStudentID[markerID]);
			int err;
			err = pthread_cond_wait(&condition, &lock);
			if(err){
				printf("pthread_cond_wait in Marker waiting grab occur error");
				abort();
			}
			if(isTimeOut()) {
				printf("%d marker %d: exits lab (timeout)\n", timenow(), markerID);
				notFullMarkerCount--;
				pthread_cond_broadcast(&condition);
				pthread_cond_broadcast(&demoCond);
				unlockGrab();
				return NULL;
			}
			//printf("unassessedStudentCount=%d", unassessedStudentCount);
			/* if(unassessedStudentCount == 0) {
				printf("%d marker %d: exits lab (finished %d jobs)\n", timenow(), markerID,  markerProjectCount[markerID]);
				notFullMarkerCount--;
				pthread_cond_broadcast(&condition);
				pthread_cond_broadcast(&demoCond);
				unlockGrab();
				return NULL;
			} */

		}
		unlockGrab();

		//printf("--before grab markerState[markerID]=%d, markerAssessStudentID[markerID] =%d\n",markerState[markerID],markerAssessStudentID[markerID]);

		/* The following line shall be printed when a marker is grabbed by a student. */
		markerProjectCount[markerID]++;
		studentID = markerAssessStudentID[markerID];
		printf("%d marker %d: grabbed by student %d (job %d)\n", timenow(), markerID, studentID, markerProjectCount[markerID]);

		lockGrab();
		if(isTimeOut()) {
			printf("%d marker %d: exits lab (timeout)\n", timenow(), markerID);
			notFullMarkerCount--;
			pthread_cond_broadcast(&condition);
			pthread_cond_broadcast(&demoCond);
			unlockGrab();
			return NULL;
		}
		studentMarkerGrabCount[studentID]++;
		pthread_cond_broadcast(&demoCond);
		unlockGrab();


		//(c)
		lockGrab();
		while(demoState[studentID] == 0) {
			int err;
			err = pthread_cond_wait(&demoCond, &lock);
			if(err){
				printf("pthread_cond_wait in Marker waiting end occur error");
				abort();
			}

		}
 		printf("%d marker %d: finished with student %d (job %d)\n", timenow(), markerID, studentID, markerProjectCount[markerID]);

		unlockGrab();


		lockGrab();
		/* if(unassessedStudentCount == 0) {
			printf("%d marker %d: exits lab (finished %d jobs)\n", timenow(), markerID,  markerProjectCount[markerID]);
			notFullMarkerCount--;
			pthread_cond_broadcast(&condition);

			unlockGrab();
			return NULL;
		} */

		if(markerProjectCount[markerID] < parameters.N) {
			idleMarkerCount++;
		} else if(markerProjectCount[markerID] == parameters.N) {
			notFullMarkerCount--;
			printf("%d marker %d: exits lab (finished %d jobs)\n", timenow(), markerID, parameters.N);
			pthread_cond_broadcast(&condition);
			pthread_cond_broadcast(&demoCond);
			unlockGrab();
			return NULL;
		}
		
     	 if(isTimeOut()) {
			printf("%d marker %d: exits lab (timeout)\n", timenow(), markerID);
			notFullMarkerCount--;
			pthread_cond_broadcast(&condition);
			pthread_cond_broadcast(&demoCond);
			unlockGrab();
			return NULL;
		}
		markerState[markerID] = 0;
		markerAssessStudentID[markerID] = -1;
		pthread_cond_broadcast(&condition);
		pthread_cond_broadcast(&demoCond);
		unlockGrab();

	}


    /*
     * When the marker exits the lab, exactly one of the following two lines shall be
     * printed, depending on whether the marker has finished all their jobs or there
     * is no time to complete another demo.
     */

    //printf("%d marker %d: exits lab (finished %d jobs)\n", timenow(), markerID, parameters.N);
    //printf("%d marker %d: exits lab (timeout)\n", timenow(), markerID);

    return NULL;
}


/*
 * A student thread. You must modify this function.
 */
void *student(void *arg) {
    /* The ID of the current student. */
    int studentID = *(int *)arg;

    /* 1. Panic! */
    printf("%d student %d: starts panicking\n", timenow(), studentID);
    panic();

	lockGrab();
	while(markerCount != parameters.M) {
		int err;
		err = pthread_cond_wait(&markerReady, &lock);
		if(err){
			printf("pthread_cond_wait in Student occur error");
			abort();
		}
	}
	unlockGrab();

    /* 2. Enter the lab. */
    printf("%d student %d: enters lab\n", timenow(), studentID);

	// is the rest of time enough
	lockGrab();
	if(isTimeOut()){
		//unassessedStudentCount--;
	    pthread_cond_broadcast(&condition);
		unlockGrab();
		printf("%d student %d: exits lab (timeout)\n", timenow(), studentID);
		return NULL;
	} else {
		//printf("begin grab");
		int grabCount = 0;

		/* 3. Grab K markers. */
		// wait for idle markers
		while(idleMarkerCount < parameters.K) {
			int err;
			err = pthread_cond_wait(&condition, &lock);
			if(err){
				printf("pthread_cond_wait in Student occur error");
				abort();
			}
			//notFullMarkerCount < parameters.K
			if(isTimeOut()) {
				//unassessedStudentCount--;
				pthread_cond_broadcast(&condition);
				unlockGrab();
				printf("%d student %d: exits lab (timeout)\n", timenow(), studentID);
				return NULL;
			}

		}


		//grab K markers
		int j = 0;
		while(j < parameters.M) {
			//notFullMarkerCount < parameters.K 
			if(isTimeOut()){
				printf("%d student %d: exits lab (timeout)\n", timenow(), studentID);
				//unassessedStudentCount--;
				pthread_cond_broadcast(&condition);
				unlockGrab();
				return NULL;
			}

			if(markerState[j] == 0 && markerProjectCount[j] < parameters.N) {
				markerAssessStudentID[j] = studentID;// assign studentID to marker j
				markerState[j] = 1;// this marker has been grabbed
				idleMarkerCount--;
				grabCount++;
				//printf("==%d student %d: grabbed marker %d\n", timenow(), studentID, j);
			}

			if(grabCount == parameters.K) {
				break;
			}
			j++;

		}


		pthread_cond_broadcast(&condition);

		unlockGrab();

		/* 4. Demo! */
		/*
		 * If the student succeeds in grabbing K markers and there is enough time left
		 * for a demo, the following three lines shall be executed in order.
		 * If the student has not started their demo and there is not sufficient time
		 * left to do a full demo, the following three lines shall not be executed
		 * and the student proceeds to step 5.
		 */


		 lockGrab();
			 while(studentMarkerGrabCount[studentID] != parameters.K) {
				int err;
				err = pthread_cond_wait(&demoCond, &lock);
				if(err){
					printf("pthread_cond_wait in Student occur error");
					abort();
				}

				if(isTimeOut()){
					printf("%d student %d: exits lab (timeout)\n", timenow(), studentID);
					//unassessedStudentCount--;
					pthread_cond_broadcast(&condition);
					unlockGrab();
					return NULL;
				}
			}

		 unlockGrab();


		printf("%d student %d: starts demo\n", timenow(), studentID);
		demo();
		printf("%d student %d: ends demo\n", timenow(), studentID);
		demoState[studentID] = 1;

		lockGrab();
			  //unassessedStudentCount--;
			  pthread_cond_broadcast(&demoCond);
			  pthread_cond_broadcast(&condition);
		unlockGrab();

		 /* 5. Exit the lab. */
		printf("%d student %d: exits lab (finished)\n", timenow(), studentID);

	}

    /*
     * Exactly one of the following two lines shall be printed, depending on
     * whether the student got to give their demo or not.
     */
    //printf("%d student %d: exits lab (timeout)\n", timenow(), studentID);
    //printf("%d student %d: exits lab (finished)\n", timenow(), studentID);

    return NULL;
}


/* The function that runs the session.
 * You MAY want to modify this function.
 */
void run() {
    int i;
    int markerID[100], studentID[100];
    pthread_t markerT[100], studentT[100];

	// pthread initialization
	pthread_cond_init(&condition, NULL);
	pthread_cond_init(&demoCond, NULL);
	pthread_cond_init(&markerReady, NULL);
	pthread_cond_init(&timeoutCond, NULL);
	pthread_mutex_init(&lock, NULL);

    printf("S=%d M=%d K=%d N=%d T=%d D=%d\n",
        parameters.S,
        parameters.M,
        parameters.K,
        parameters.N,
        parameters.T,
        parameters.D);
    gettimeofday(&starttime, NULL);  /* Save start of simulated time */

	notFullMarkerCount = 0;
	markerCount = 0;
    idleMarkerCount = 0;
	//unassessedStudentCount = parameters.S;


    /* Create S student threads */
    for (i = 0; i<parameters.S; i++) {
        studentID[i] = i;
		demoState[i] = 0;
		studentMarkerGrabCount[i] = 0;
        pthread_create(&studentT[i], NULL, student, &studentID[i]);
    }
    /* Create M marker threads */
    for (i = 0; i<parameters.M; i++) {
        markerID[i] = i;
		markerState[i] = 0;
		markerProjectCount[i] = 0;
		markerAssessStudentID[i] = -1;
        pthread_create(&markerT[i], NULL, marker, &markerID[i]);
    }



    /* With the threads now started, the session is in full swing ... */
    delay(parameters.T - parameters.D);
	pthread_cond_broadcast(&condition);

    /*
     * When we reach here, this is the latest time a new demo could start.
     * You might want to do something here or soon after.
     */

    /* Wait for student threads to finish */
    for (i = 0; i<parameters.S; i++) {
        pthread_join(studentT[i], NULL);
    }

    /* Wait for marker threads to finish */
    for (i = 0; i<parameters.M; i++) {
        pthread_join(markerT[i], NULL);
    }
}



/*
 * main() checks that the parameters are ok. If they are, the interesting bit
 * is in run() so please don't modify main().
 */
int main(int argc, char *argv[]) {
    if (argc < 6) {
        puts("Usage: demo S M K N T D\n");
        exit(1);
    }
    parameters.S = atoi(argv[1]);
    parameters.M = atoi(argv[2]);
    parameters.K = atoi(argv[3]);
    parameters.N = atoi(argv[4]);
    parameters.T = atoi(argv[5]);
    parameters.D = atoi(argv[6]);
    if (parameters.M > 100 || parameters.S > 100) {
        puts("Maximum 100 markers and 100 students allowed.\n");
        exit(1);
    }
    if (parameters.D >= parameters.T) {
        puts("Constraint D < T violated.\n");
        exit(1);
    }
    if (parameters.S*parameters.K > parameters.M*parameters.N) {
        puts("Constraint S*K <= M*N violated.\n");
        exit(1);
    }

    // We're good to go.
    run();
    return 0;
}
