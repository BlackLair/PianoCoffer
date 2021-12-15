
// 라즈베리파이 첫 부팅 시 다음 명령어 입력 : sudo rfcomm bind rfcomm0 98:D3:31:F4:19:CB

#include <softTone.h>
#include <time.h>
#include <softPwm.h>

#include <stdio.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wiringPiI2C.h>






////////////////////////////GPIO PIN NUMBER DEFINITION///////////////////////

#define DHTPIN 17 // temp
#define MOTION 13 // 동작 인식 센서 
#define LED_PIN
#define SERVO_PIN 27	// 서보모터
#define SWITCH_PIN
#define BUZZER_PIN 26 // temp
#define BUZZER_POWER 19 // power supply for buzzer
//////   건반 충돌감지 센서  //////////////
#define SENSOR_C 14 
#define SENSOR_CS 15
#define SENSOR_D 18
#define SENSOR_DS 23
#define SENSOR_E 24
#define SENSOR_F 25
#define SENSOR_FS 8
#define SENSOR_G 7
#define SENSOR_GS 1
#define SENSOR_A 12
#define SENSOR_AS 16
#define SENSOR_B 20
#define SENSOR_CH 21
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////SPEC DEFINITION///////////////////
#define MAX_KEY_NUM 13  // 총 건반 개수 : 완성 시 13으로 편집 필요
#define MAXTIMINGS 83
#define BLUE_PORT "/dev/rfcomm0"
#define BAUD_RATE 9600
#define ALLOWED_OFFSET 250	// 단일 타이밍 길이 허용 범위
////////////////////////////////////PASSWORD STRUCT DEFINITION///////////////

typedef struct SONG_NODE { // 비밀 곡의 단일 타이밍 정보 노드
	double width; 	// 타이밍의 길이
	uint16_t state;			// 건반의 상태
	struct SONG_NODE* nextNode;	// 다음 타이밍 노드
}SONG_NODE;
typedef struct SONG_HEADER { // 비밀 곡의 헤드 노드
	SONG_NODE* headNode; // 비밀 곡의 헤드 노드
	SONG_NODE* compNode;
	int state_num;		// 비밀 곡의 단일 타이밍 노드 개수
}SONG_HEADER;
/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////GLOBAL VARIABLES//////////////////////////
// 건반 충돌감지 센서 핀 번호
const int SENSOR_TABLE[13]={SENSOR_C, SENSOR_CS, SENSOR_D, SENSOR_DS, SENSOR_E, SENSOR_F, SENSOR_FS, SENSOR_G, SENSOR_GS, SENSOR_A, SENSOR_AS, SENSOR_B, SENSOR_CH};
// 부저 소리 주파수
const int FREQ_TABLE[13] = {523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988, 1047};
// 온습도 센서 데이터
int dht11_dat[5]={0,};
int min_temp=26;	// FAN 작동 위한 최소 온도 설정 값
int min_humi=20;	// FAN 작동 위한 최소 온도 설정 값
int isTempDisplay=1; // 1 : 온습도 출력 가능
					//  0 : 온습도 출력 불가능
int isPlayingMode=1; // 1 : 연주 모드
int current_state=0; 	// -1 : 경고 상태
						//  0 : 평상시
						//  1 : 비밀 곡 인증완료
						//  2 : 지문인식 완료/메인 메뉴
						//  3 : 비밀 곡 설정상태
						//  4 : 지문 인식 설정 상태
						//  5 : 온습도 설정 상태
int temp_cnt=0;		// 임시 변수..

int dev=0;					// 시리얼 통신 식별 번호

int TOTAL_THREAD_NUM=1;		// 총 스레드 개수

const char menutitle[4][18] = {"Set secret song", "Set fingerprint", "Set Humi/Temp", "Exit"};

int motion_detected=0;		// 연속 동작 감지 횟수
int motion_checktime_width=0; // 지문 인증 단계에서 모션 인식 측정 시각 판별
int warning_cnt=0;			// 경고모드 시 경고음 생성용 카운터
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////FUNCTION PROTOTYPE//////////////////////
int softToneCreate(int pin);
void softToneWrite(int pin, int freq);

// LCD Control Code //
extern void lcd_write_word( int );
extern void lcd_send_command( int  );
extern void lcd_send_data( int  );
extern void lcd_init( void );
extern void lcd_clear( void );
extern void lcd_write( int , int , char * );
//////////////////////
// Bluetooth Serial //
/*
char* gets(char*);
void serialPuts(int, char*);
void serialPutchar(int, char);
char serialGetchar(int);
void serialFlush(int);
int serialOpen(char*, int);
int serialClose(int);
*/
///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////SONG NODE FUNCTIONS DEFINITION/////////////
SONG_NODE* create_node(double ms, uint16_t state){ // 1개의 비밀 곡 단일 타이밍 생성
	SONG_NODE* new_node;
	new_node=(SONG_NODE*)malloc(sizeof(SONG_NODE)); 	// 단일 노드 메모리 할당
	new_node->width=ms;								// 상태가 유지된 시간 저장
	new_node->state=state;							// 상태 저장
	new_node->nextNode=NULL;						// 다음 타이밍 노드를 NULL값으로 설정
	return new_node;
}
SONG_HEADER* create_header(){ 		// 비밀 곡 헤더 생성
	SONG_HEADER* new_header;
	new_header=(SONG_HEADER*)malloc(sizeof(SONG_HEADER));
	new_header->headNode=NULL;
	new_header->compNode=NULL;
	new_header->state_num=0;
	return new_header;
}
int insert_node(SONG_HEADER *header, SONG_NODE *new_node){ // 비밀 곡 단일 타이밍을 연결 리스트에 결합
	SONG_NODE* pNode;
	if(header->headNode==NULL){ // 헤더가 비어있을 경우
		if(new_node->state | 0x1FFF){	// 비밀 곡의 첫 번째 타이밍은 모든 건반이 뗀 상태를 가지지 않도록 함
			header->headNode=new_node;
		}
		else{					
			free(new_node); 
		}
		return 0;
	}
	pNode=header->headNode;
	while(pNode->nextNode!=NULL)
		pNode=pNode->nextNode;
	pNode->nextNode=new_node;
	return 0;
}


// 반환값 -1	: 단일 타이밍 불일치
// 반환값  0	: 단일 타이밍 일치
// 반환값  1	: 비밀 곡 일치
int compare_singleTiming(SONG_NODE **compNode, SONG_NODE **headNode, double ms, uint16_t state){ // 비밀 곡의 단일 노드와 건반의 바뀐 상태를 검사
	double max_limit, min_limit;
	if((*compNode)!=NULL){
		max_limit=((*compNode)->width)+ALLOWED_OFFSET;				// 단일 타이밍 길이의 허용 범위 최대값
		min_limit=((*compNode)->width)-ALLOWED_OFFSET;				// 단일 타이밍 길이의 허용 범위 최솟값
	}
	printf("diff : %.3f state : %d\n", ms, state);
	if((*compNode)!=NULL){
		printf("compdff %.3lf  compstate ", ((*compNode)->width));
		if((*compNode)->nextNode!=NULL)
			printf("%d", ((*compNode)->nextNode)->state);
		printf("\n");
	}
	
	if((*compNode)==NULL){ 				// 가장 첫 번째 단일 타이밍 검사
		if(state!=(*headNode)->state){	// 눌린 건반 상태가 가장 첫 번째 음과 다를 경우
			printf("first state is not correct\n");
			return -1;
		}
	}
	
	else if((*compNode)->nextNode!=NULL){// 중간 단일 타이밍을 점검하고 다음 타이밍 노드가 남아있을 경우
		printf("compstate : %d\n", ((*compNode)->nextNode)->state);
		if(state!=((*compNode)->nextNode)->state){	// 바뀐 상태가 다음 노드의 건반 상태와 다를 경우
			if(state==(*headNode)->state){
				(*compNode)=(*headNode);
				printf("first state is correct\n");
				return 0;
			}
			(*compNode)=NULL;									// 비교 대상 노드를 NULL로 초기화
			printf("mid state is not correct\n");
			return -1;
		}
		if(ms>max_limit || ms<min_limit){					// 직전 상태의 유지시간이 현재 타이밍 노드의 유지시간과 다를 경우
			printf("mid width is not correct\n");
			(*compNode)=NULL;									// 비교 대상 노드를 NULL로 초기화
			return -1;
		}
	}
	else if((*compNode)->nextNode==NULL){			// 마지막 단일 타이밍 검사
		if(ms>max_limit || ms<min_limit){
			(*compNode)=NULL;									// 비교 대상 노드를 NULL로 초기화
			printf("final width is not correct\n");
			return -1;
		}
		
	}
	
	/////////////// Compare Success ! //////////////
	if((*compNode)==NULL){							// 첫 번째 음계 상태 비교 성공인 경우
		(*compNode)=(*headNode);
		printf("first state is correct\n");
		return 0;
	}
	else if((*compNode)->nextNode !=NULL){			// 비교 성공 시 다음 음계 비교 준비
		(*compNode)=(*compNode)->nextNode;
		printf("single timing is correct\n");
		return 0;
	}
	else{												// 비밀번호 모두 일치
		printf("all timings are correct\n");
		(*compNode)=NULL;
		return 1;
	}
	printf(" unexpected case...\n");
	return -2; // 알 수 없는 상태
}
SONG_HEADER* read_password(){ // 텍스트 파일에 저장된 비밀번호를 읽어 연결 리스트로 생성
	SONG_HEADER* new_header;
	SONG_NODE* new_node, *temp_node;
	SONG_NODE* temp_node2;
	double ms;
	int state_num, i;
	uint16_t keystate;
	FILE* fPW;
	new_header=create_header(); // 새로운 헤더 구조체 생성
	fPW=fopen("password.txt", "r");
	printf("password.txt open\n");
	fscanf(fPW, "%d", &state_num); 	// 타이밍 개수를 읽어옴
	
	new_header->state_num=state_num;
	
	for(i=0; i<new_header->state_num; i++){			// 단일 타이밍 개수만큼 반복
		fscanf(fPW, "%lf", &ms);						// 단일 타이밍 길이
		fscanf(fPW, "%d", &keystate);					// 단일 타이밍 건반 상태
		insert_node(new_header, create_node(ms, keystate));
	}
	temp_node=new_header->headNode;
	
	fclose(fPW);
	return new_header; 								// 비밀 곡 헤더 구조체 반환
}
void write_password(SONG_HEADER* header){
	SONG_NODE *temp_node;
	FILE* fPW;
	int i;
	fPW=fopen("password.txt", "w+");		// 비밀 곡 덮어쓰기
	
	fprintf(fPW, "%d", header->state_num);
	temp_node=header->headNode;
	for(i=0; i<header->state_num; i++){
		fprintf(fPW, " ");
		fprintf(fPW, "%.3lf", temp_node->width);
		fprintf(fPW, " ");
		fprintf(fPW, "%d", temp_node->state);
		printf("%.3lf %d\n", temp_node->width, temp_node->state);
		temp_node=temp_node->nextNode;
	}
	fclose(fPW);
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////SUB FUNCTION DEFINITION////////////////////
void Change_FREQ(unsigned int freq){ 	// 부저의 소리 음계 변경
	softToneWrite(BUZZER_PIN, freq);
}
void STOP_FREQ(void){					// 부저의 소리 멈춤
	softToneWrite(BUZZER_PIN, 0);
}
void Buzzer_Init(void){					// 부저 초기화
	softToneCreate(BUZZER_PIN);			// 부저 핀 초기화
	pinMode(BUZZER_POWER, OUTPUT);		// 부저 전원공급용 GPIO 출력
	digitalWrite(BUZZER_POWER, HIGH);
	STOP_FREQ();
}

void myDelayms(unsigned int msec){				// delay함수 대신 사용할 함수
	clock_t past, now;
	double debounce=0, limit;
	past=(double)clock(); 						// 함수 시작 시 시간 가져옴
	limit=((double)msec/1000)*CLOCKS_PER_SEC;	// 매개변수로 받은 값으로 한계치 계산
	limit*=TOTAL_THREAD_NUM;					// clock은 스레드 수만큼 빨라짐
	while(limit>debounce){						// 한계치만큼 시간 경과시 반복문 종료
		debounce=(double)clock()-past;			// 함수가 시작한 시점부터 경과한 시간 계산
	}
	//printf("%.3lf초 경과. 함수 종료\n", (double)debounce/CLOCKS_PER_SEC);
}
void myDelayus(unsigned int usec){				// delayMicrosecond함수 대신 사용할 함수
	clock_t past, now;
	double debounce=0, limit;
	past=(double)clock();
	limit=((double)usec/1000000)*CLOCKS_PER_SEC;
	limit*=TOTAL_THREAD_NUM;					// clock은 스레드 수만큼 빨라짐
	while(limit>debounce){
		debounce=(double)clock()-past;
	}

}
void itoa(char* buf, int val){
	sprintf(buf, "%d", val);
}
void read_dht11_dat(){ 							// 온습도센서 읽기
	uint8_t laststate=HIGH;
	uint8_t counter=0;
	uint8_t j=0, i;
	int ctemp, chumi;
	char buffer[18]="";
	char intbuffer[3]="";
	char send_msg[4];
	static unsigned int cnt=-1;
	cnt++;
	if(cnt%15==0){
		dht11_dat[0]=dht11_dat[1]=dht11_dat[2]=dht11_dat[3]=dht11_dat[4]=0;
		pinMode(DHTPIN, OUTPUT);
		digitalWrite(DHTPIN, LOW);					// 온습도 센서에 통신 시작 알림
		delay(18);
		digitalWrite(DHTPIN, HIGH);
		delayMicroseconds(40);
		pinMode(DHTPIN, INPUT);
		for(i=0; i<MAXTIMINGS; i++){
			counter=0;
			while(digitalRead(DHTPIN)==laststate){
				counter++;
				delayMicroseconds(1);
				if(counter==255) break;
			}
			laststate=digitalRead(DHTPIN);
			if(counter==255) break;
			if((i>=4) && (i%2==0)){					// 실제 데이터 읽어옴
				dht11_dat[j/8]<<=1;
				if(counter>26) dht11_dat[j/8]|=1;
				j++;
			}
		}
		// 습도 dht11_dat[0]과 min_humi 비교, 온도 dht11_dat[2]와 min_temp 비교하여 FAN 제어하는 코드 작성하기
		
		if((j>=40)&&(dht11_dat[4]==((dht11_dat[0]+dht11_dat[1]+dht11_dat[2]+dht11_dat[3])&0xff))){ // 데이터 정상적으로 수신
			if(current_state==0){
				lcd_write(0, 3, "                  ");
				myDelayms(5);
				memset(buffer, '\0', sizeof(char)*18);
				memset(intbuffer, '\0', sizeof(char)*3);
				strcat(buffer, "T/H : ");
				itoa(intbuffer, dht11_dat[2]);
				strcat(buffer, intbuffer);
				strcat(buffer, ".");
				itoa(intbuffer, dht11_dat[3]);
				strcat(buffer, intbuffer);
				strcat(buffer, "*C ");
				itoa(intbuffer, dht11_dat[0]);
				strcat(buffer, intbuffer);
				strcat(buffer, ".");
				itoa(intbuffer, dht11_dat[1]);
				strcat(buffer, intbuffer);
				strcat(buffer, "%");
				lcd_write(0, 3, buffer);
			}memset(intbuffer, '\0', sizeof(char)*3);
			printf("humidity=%d.%d %% Temperature = %d.%d *C \n", dht11_dat[0], dht11_dat[1], dht11_dat[2], dht11_dat[3]);
			chumi=dht11_dat[0];
			ctemp=dht11_dat[2];
			printf("chumi %d min %d\n", chumi, min_humi);
			printf("ctemp %d min %d\n", ctemp, min_temp);
			if((chumi>=min_humi) || (ctemp>=min_temp)){
				serialPuts(dev, "1");
				printf("Fan On\n");
			}else{
				serialPuts(dev, "2");
				printf("Fan Off\n");
			}
		}
		else printf("Data get failed\n");		// 데이터 이상 수신
	}
}

void open_motor(){
	softPwmWrite(SERVO_PIN, 5);
}
void close_motor(){
	softPwmWrite(SERVO_PIN, 15);
}

uint16_t read_keystate(){ // 건반 상태 읽기
	int i;
	uint16_t keystate=0;						// 16비트 중 13개 비트에 각 건반의 상태를 0 또는 1로 저장
	for(i=0; i<MAX_KEY_NUM; i++){				// 저음부터 순서대로 건반의 상태 읽어옴
		if(digitalRead(SENSOR_TABLE[i])==LOW){	// 충돌 감지 센서는 LOW ACTIVE로 작동함
			keystate|=(1<<i);					// 눌린 건반에 해당되는 비트를 1로 변경
		}
	}
	return keystate;							// 건반 상태 반환
	
}

void print_currtime(){
	struct tm *currtime;	// 현재 시간 저장 구조체
	char buf[20];			// 현재 시간 출력 위한 문자열
	char intbuf[4];			// 현재 시간 각 정수 임시 저장 버퍼 
	time_t t;
	t=time(NULL);				// 
	currtime=localtime(&t);		// 현재 지역 시간 가져옴
	itoa(intbuf, currtime->tm_hour);
	if(currtime->tm_hour<10)
		strcat(buf, "0");
	strcat(buf, intbuf);
	strcat(buf, ":");
	itoa(intbuf, currtime->tm_min);
	if(currtime->tm_min<10)
		strcat(buf, "0");
	strcat(buf, intbuf);
	strcat(buf, ":");
	itoa(intbuf, currtime->tm_sec);
	if(currtime->tm_sec<10)
		strcat(buf, "0");
	strcat(buf, intbuf);
	lcd_write(5, 1, buf);
}
int check_motion(){	// 0 반환 시 그냥 종료, 1 반환 시 평상시 모드로 전환됨
	char ch=0;
	printf("motion : %d\n", motion_detected);
	if(digitalRead(MOTION)==HIGH){		// 잠긴 상태에서 모션 감지
		motion_detected++;
		
		if(motion_detected>6){		// 잠긴 상태에서 움직임 7초 이상 감지 시 경고 모드
			current_state=-1;
			lcd_clear();
			lcd_write(3, 0, "WARNING!!!");
			lcd_write(3, 1, "WARNING!!!");
			lcd_write(3, 2, "WARNING!!!");
			lcd_write(3, 3, "WARNING!!!");
			serialPuts(dev, "3");
			while(ch=serialGetchar(dev)){
				warning_cnt++;
				if(warning_cnt%7>2)
					Change_FREQ(1700);
				else
					Change_FREQ(1500);
				printf("%c\n", ch);
				if(ch=='y'){			// 지문인식 성공 시
					STOP_FREQ();
					lcd_clear();
					lcd_write(3, 0, "Finger Matched.");
					myDelayms(2000);
					current_state=0;
					serialFlush(dev);
					motion_detected=0;
					isTempDisplay=1;
					isPlayingMode=1;
					return 1;
				}
			}
		}
		else
			return 0;
	}
	else{
		motion_detected=0;
		return 0;
	}
}

void new_finger(int i){		// i : 1: 지문 변경  0: 지문 없을 때 첫 등록
	char ch;
	printf("init set fingerprint\n");
	lcd_clear();
	lcd_write(1, 0, "Tag new Finger...");
	serialFlush(dev);
	myDelayms(300);
	if(i==1)
		current_state=4;
	serialPuts(dev, "4");
	while(ch=serialGetchar(dev)){
		printf("%c\n", ch);
		if(ch=='n'){
			lcd_clear();
			lcd_write(1, 0, "Tag new Finger...");
			serialFlush(dev);
		}
		if(read_keystate()==1 && i==1){ // 지문 인식 취소
			serialPuts(dev, "c");
			lcd_clear();
			lcd_write(0, 0, "Cancled.");
			myDelayms(2000);
			current_state=2;
			serialFlush(dev);
			break;
		}
		if(ch=='r'){ //지문 2차 등록
			lcd_clear();
			lcd_write(2, 0, "Remove Finger..");
			serialFlush(dev);
		}
		else if(ch=='t'){
			lcd_clear();
			lcd_write(2, 0, "Place same finger");
			lcd_write(2, 1, "again...");
			serialFlush(dev);
		}
		else if(ch=='y'){		// 지문 등록 성공
			lcd_clear();
			lcd_write(2, 0, "New finger stored!");
			myDelayms(2000);
			if(i==1)
				current_state=2;
			serialFlush(dev);
			break;
		}
		else if(ch=='x'){
			lcd_clear();
			lcd_write(2, 0, "finger not match..");
			myDelayms(2000);
			lcd_clear();
			lcd_write(1, 0, "Tag new Finger...");
			serialFlush(dev);
		}
		else if(ch=='z'){
			lcd_clear();
			lcd_write(2, 0, "Error...");
			myDelayms(2000);
			lcd_write(1, 0, "Tag new Finger...");
			serialFlush(dev);
		}
	}
	serialFlush(dev);
}

////////////////////////////////////////////////////////////////////////////

int main(){
	/////////////////////////////// Variables ////////////////////////
	uint16_t keystate=0, keystate_past=0;	// 
	uint16_t keystate_xor=0;				// keystate와 keystate_past의 xor 결과:상태가 변경된 건반
	int i;
	clock_t past=0, now=0, lcd_renew=0; 	// 건반 상태 바뀐 시작지점과 끝점 시간, lcd 갱신타이밍
	double diff=0;			// 건반 상태 유지 시간 길이
	int compare_result=0;	// 건반과 비밀 곡 단일 타이밍 비교 결과
	SONG_HEADER* pwHeader;	// 비밀 곡 연결 리스트
	pthread_t humithread_id;// 온습도를 읽어오는 쓰레드 id
	char ch;				// 블루투스 수신 데이터 버퍼
	char intbuf[4];
	SONG_NODE* temp_node;
	SONG_NODE* temp_node2;
	int isSettingEnd=0;
	/////////////////////// Initial Processing ////////////////////////
	
	if(wiringPiSetupGpio()==-1){		// GPIO 초기화
		printf("gpio setup failed\n");
		return -1;
	}
	if((dev = serialOpen(BLUE_PORT, BAUD_RATE))<0) // 아두이노와 블루투스 연결
	{
		printf(" serial open failed.\n");
		return -1;
	}
	lcd_init();		// LCD 초기화
	lcd_clear();	// LCD 화면 지움
	for(i=0; i<MAX_KEY_NUM; i++){		// 건반의 충돌센서 GPIO 입력 모드 설정
		pinMode(SENSOR_TABLE[i], INPUT);
	}
	pinMode(MOTION, INPUT);				// 동작 인식 센서 
	softPwmCreate(SERVO_PIN, 0, 200);		// 서보모터 PWM
	myDelayms(20);
	close_motor();							// 서보모터 잠금
	Buzzer_Init();						// 부저 초기화
	printf("CPS = %d\n", CLOCKS_PER_SEC);	// CPU의 초당 클록 수 출력
	
	pwHeader=read_password();				// 저장된 비밀 곡 txt 파일을 읽어 연결 리스트로 생성
	lcd_write(3, 0, "hello, Pandora");
	myDelayms(500);
	past=clock()/TOTAL_THREAD_NUM;
	lcd_renew=clock()/TOTAL_THREAD_NUM;
	//////////////////////////////////////////////////////////////////
	ch=serialGetchar(dev);	// 지문 정보가 이미 존재하는지 확인
	serialFlush(dev);
	if(ch=='f'){
		new_finger(0);
	}
	
	
	//////////////////////////////////////////////////////////////////
	while(1){
		keystate=read_keystate(); // 건반 상태 읽기
		
		if(keystate!=keystate_past){			// 건반의 상태에 변화가 생겼을 경우
			now=clock()/TOTAL_THREAD_NUM; 				// 현재 clock 틱 수를 받아옴
			diff=(double)(now-past)/1000;		// 이전 마지막으로 건반 상태가 바뀐 시점으로부터 현재까지 지난 시간을 계산
			if(diff>50){						// 채터링 현상 방지를 위해 최소 50ms가 유지되었을 때만 동작
				if(isPlayingMode==1){			//연주모드일 경우
					if(current_state==0){		// 잠긴 상태 연주 모드
						compare_result=compare_singleTiming(&(pwHeader->compNode), &(pwHeader->headNode), diff, keystate);	// 비밀 곡과 비교. 건반 상태가 바뀌기까지 걸린 시간과 바뀌게 된 건반 상태를 비교
						printf("%d\n", compare_result);	// 비교 결과(디버깅)
						keystate_xor=keystate ^ keystate_past;	// 건반 상태가 바뀐 비트만 1
						for(i=0; i<MAX_KEY_NUM; i++){	// 건반 개수만큼 반복
							if(keystate  & (1<<i)){		// 건반이 눌린 상태일 경우
								Change_FREQ(FREQ_TABLE[i]);	// 눌린 건반에 해당되는 주파수 부저로 재생
								break;					// 반복문 종료
							}
							STOP_FREQ();				// 눌린 건반이 없을 경우 재생 중지
						}
					}
					else if(current_state==3){				// 비밀 곡 변경: 연주 녹음
						if(pwHeader->headNode==NULL){
							for(i=0; i<MAX_KEY_NUM; i++){	// 건반 개수만큼 반복
								if(keystate  & (1<<i)){		// 건반이 눌린 상태일 경우
									Change_FREQ(FREQ_TABLE[i]);	// 눌린 건반에 해당되는 주파수 부저로 재생
									break;					// 반복문 종료
								}
								STOP_FREQ();				// 눌린 건반이 없을 경우 재생 중지
							}
							if(keystate!=0){
								insert_node(pwHeader, create_node(0, keystate));
								(pwHeader->state_num)++;
								temp_node=pwHeader->headNode;
							}
						}else{
							for(i=0; i<MAX_KEY_NUM; i++){	// 건반 개수만큼 반복
								if(keystate  & (1<<i)){		// 건반이 눌린 상태일 경우
									Change_FREQ(FREQ_TABLE[i]);	// 눌린 건반에 해당되는 주파수 부저로 재생
									break;					// 반복문 종료
								}
								STOP_FREQ();				// 눌린 건반이 없을 경우 재생 중지
							}
							insert_node(pwHeader, create_node(0, keystate));
							(pwHeader->state_num)++;
							temp_node->width=diff;
							temp_node=temp_node->nextNode;
						}
						
						
					}
				}
				if(current_state==2){	// 메인 메뉴 상태에서 기능 선택했을 경우
				
					if(keystate & 0x0001){	// 비밀 곡 변경 기능 선택 시
						printf("init set secret song\n");
						lcd_clear();
						lcd_write(0, 0, "ready...");
						myDelayms(2000);
						lcd_write(0, 0, "Play new secret");
						lcd_write(0, 1, "song...\n");
						current_state=3;
						temp_node=pwHeader->headNode;
						temp_node2=temp_node->nextNode;
						while(temp_node2!=NULL){
							free(temp_node);
							temp_node=temp_node2;
							temp_node2=temp_node->nextNode;
						}
						free(temp_node);
						free(pwHeader);
						pwHeader=NULL;
						pwHeader=create_header();
						isPlayingMode=1;
					}
					else if(keystate & 0x0002){	//// 지문 변경 기능 선택 시
						new_finger(1);
					}
					else if(keystate & 0x0004){	// 온습도 설정 기능 선택 시
						printf("init set temphumi\n");
						
						current_state=5;
					}
					else if(keystate & 0x0008){	// 메인메뉴 나가고 금고 잠그기 
						printf("exit\n");
						lcd_clear();
						lcd_write(2, 0, "Locked.");
						close_motor();
						myDelayms(2000);
						isPlayingMode=1;
						current_state=0;
						isTempDisplay=1;
					}
				}
				else if(current_state==5){ // 온습도 설정
					if(keystate&0x0001)
						min_temp--;
					else if(keystate&0x0002)
						min_temp++;
					else if(keystate&0x0004)
						min_humi--;
					else if(keystate&0x0008)
						min_humi++;
					if(keystate!=0){
						lcd_clear();
						lcd_write(0, 0, "set Temp and Humi..");
						itoa(intbuf, min_temp); // temp
						lcd_write(2, 1, "<<");
						lcd_write(5, 1, intbuf);
						lcd_write(8, 1, "*C ");
						lcd_write(11, 1, ">>");
						itoa(intbuf, min_humi); // humi
						lcd_write(2, 2, "<<");
						lcd_write(5, 2, intbuf);
						lcd_write(8, 2, "%  ");
						lcd_write(11, 2, ">>");
					}
					if(keystate&0x0010){
						current_state=2;
						lcd_clear();
						lcd_write(6, 0, "Confirmed.");
						myDelayms(2000);
					}
				}
				past=now;
				myDelayms(50);
			}
		}
		keystate_past=keystate;			// 건반 상태를 다음 상태 변경 인식을 위해 저장
		if((clock()/TOTAL_THREAD_NUM-lcd_renew)/1000 > 1000){	// 1초마다 1회 실행.
			if(current_state==0){
				print_currtime();
				lcd_write(0, 0, "   Pandora's Box   ");
				check_motion();
			}
			if(current_state==3){	// 5초 이상 건반을 누르지 않을 경우 설정 종료
				if((clock()/TOTAL_THREAD_NUM-past)/1000>=5000 && keystate==0){
					if(temp_node!=NULL){
						if(temp_node->width==0){
							temp_node2=pwHeader->headNode;
							while(temp_node2->nextNode!=temp_node)
								temp_node2=temp_node2->nextNode;
							free(temp_node);
							temp_node2->nextNode=NULL;
							(pwHeader->state_num)--;
							write_password(pwHeader);
							lcd_clear();
							lcd_write(0, 0, "secret song changed");
							myDelayms(2000);
							temp_node=pwHeader->headNode;
							temp_node2=temp_node->nextNode;
							while(temp_node2!=NULL){
								free(temp_node);
								temp_node=temp_node2;
								temp_node2=temp_node->nextNode;
								
							}
							free(temp_node);
							free(pwHeader);
							pwHeader=NULL;
							temp_node=NULL;
							pwHeader=read_password();
						}
						else{
							free(pwHeader);
							pwHeader=read_password();
							lcd_clear();
							lcd_write(0, 0, "secret song not");
							lcd_write(0, 1, "changed");
							myDelayms(2000);
						}
						
						keystate=0;
						current_state=2;
						isPlayingMode=0;
					}
				}
			}
			if(current_state==2){				// 메인 메뉴 출력
				lcd_write(0, 0, (char*)menutitle[0]);
				lcd_write(0, 1, (char*)menutitle[1]);
				lcd_write(0, 2, (char*)menutitle[2]);
				lcd_write(0, 3, (char*)menutitle[3]);
			}
			read_dht11_dat();
			lcd_renew=clock()/TOTAL_THREAD_NUM;
			
		}
		if(compare_result==1&&current_state==0){// 비밀번호 일치 성공 시
			current_state=1;
			compare_result=-1;
			isPlayingMode=0;
			isTempDisplay=0;
			while(current_state==1){ 
				lcd_clear();
				lcd_write(2, 0, "Song Verified!");
				lcd_write(2, 1, "Tag Your Finger...");
				lcd_renew=clock()/TOTAL_THREAD_NUM;
				serialPuts(dev, "3");
				while(ch=serialGetchar(dev)){
					printf("%c\n", ch);
					if((motion_checktime_width++)%9==0){	// 1초에 1회 모션 점검
						printf("check...n");
						if(check_motion()==1)	// 경고 발생 및 정지 성공 시
							break;
					}
					if(read_keystate()==1){ // 지문 인식 취소
						serialPuts(dev, "c");
						lcd_clear();
						lcd_write(0, 0, "Cancled.");
						myDelayms(2000);
						current_state=0;
						isPlayingMode=1;
						isTempDisplay=1;
						serialFlush(dev);
						break;
					}
					if(ch=='x'){ // 지문 인식 실패
						lcd_clear();
						lcd_write(2, 0, "Finger not Found..");
						myDelayms(3000);
						lcd_clear();
						lcd_write(2, 0, "Song Verified!");
						lcd_write(2, 1, "Tag Your Finger...");
						serialFlush(dev);
					}
					else if(ch=='y'){ //지문 인식 성공
						lcd_clear();
						lcd_write(2, 0, "Finger Match!");
						open_motor();
						Change_FREQ(659);
						myDelayms(300);
						Change_FREQ(784);
						myDelayms(300);
						Change_FREQ(1047);
						myDelayms(600);
						STOP_FREQ();
						
						current_state=2;
						isTempDisplay=0;
						
						serialFlush(dev);
						break;
					}
				}
				serialFlush(dev);
			}
		}
		
		
		
	}
	serialClose(dev);
	return 0;
}
