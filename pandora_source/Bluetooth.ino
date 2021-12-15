#include <SoftwareSerial.h> 
#include <Adafruit_Fingerprint.h>

//SoftwareSerial BTSerial(2, 3); // 소프트웨어 시리얼 (TX,RX) 블루투스 모듈 상:(TXD, RXD)

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3);
#else
// On Leonardo/M0/etc, others with hardware serial, use hardware serial!
// #0 is green wire, #1 is white
#define mySerial Serial3

#endif


Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
///////////////// 핀 번호 할당
const int FAN_PIN = 53;
const int VCCSUP = 13;
/////////////////

///////////////// 상태 플래그
int LED = 0;
int FAN = 0;
/////////////////

///////////////// 기타 변수
byte recv[32];
/////////////////

int init_finger(){    // 지문 인식 센서 초기화
Serial.println("\n\nAdafruit finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  finger.getTemplateCount();      // 지문 인식 센서에 저장된 지문 데이터의 개수 반환

  if (finger.templateCount == 0) { // 등록된 지문이 없을 경우
    delay(1000);
    Serial1.write("f\n");         // 라즈베리파이에서 'f'를 받으면 지문 등록 명령이 회신됨
    return -1;
  }
  else {        // 등록된 지문이 있을 경우
    Serial1.write("t\n");
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
  }
}
uint8_t getFingerprintEnroll() {    // 지문 등록 함수

  int p = -1;
  int id=1;                         // 한 번에 1개의 지문만 저장할 수 있기 때문에 지문 번호는 1번을 고정으로 사용
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if(Serial1.available()){
      byte dt=Serial1.read();
      Serial1.flush();
      if(dt=='c'){                // 새 지문 등록 취소 명령을 받으면 0x36 반환. 지문 등록 기능 종료
        return 0x36;
      }
    }
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!
  // 오류가 발생하면 'z' 문자를 라즈베리파이에 전송

  p = finger.image2Tz(1);
  switch (p) {                // 지문 변환 성공
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;                  // 지문 변환 실패
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      Serial1.write("z\n");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:  // 통신 오류
      Serial.println("Communication error");
      Serial1.write("z\n");
      return p;
    case FINGERPRINT_FEATUREFAIL:     // 지문 변환 실패
      Serial.println("Could not find fingerprint features");
      Serial1.write("z\n");
      return p;
    case FINGERPRINT_INVALIDIMAGE:  // 유효하지 않은 이미지
      Serial.println("Could not find fingerprint features");
      Serial1.write("z\n");
      return p;
    default:
      Serial.println("Unknown error");  // 알 수 없는 에러
      Serial1.write("z\n");
      return p;
  }

  Serial.println("Remove finger");
  Serial1.write("r\n");           // 지문 이미지 획득에 성공했을 경우
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again"); 
  Serial1.write("t\n");           // 정확도를 위해 1회 더 지문 이미지를 스캔
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:    // 이미지 변환 성공
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      Serial1.write("z\n");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      Serial1.write("z\n");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      Serial1.write("z\n");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      Serial1.write("z\n");
      return p;
    default:
      Serial.println("Unknown error");
      Serial1.write("z\n");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {      // 1번째 스캔 이미지와 2번째 스캔 이미지가 일치할 경우
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    Serial1.write("z\n");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    Serial1.write("x\n");
    return p;
  } else {
    Serial.println("Unknown error");
    Serial1.write("z\n");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);    // 지문 이미지를 지문 센서에 저장 시도
  if (p == FINGERPRINT_OK) {    // 저장 성공 시
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    Serial1.write("z\n");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    Serial1.write("z\n");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    Serial1.write("z\n");
    return p;
  } else {
    Serial1.write("z\n");
    Serial.println("Unknown error");
    return p;
  }
  Serial1.write("y\n");     // 지문 등록에 성공했다고 라즈베리파이에 알림
  return true;
}
void setup(){             // 아두이노 리셋 시 1회 실행 초기화 단계
  Serial.begin(9600);     // 디버깅을 위해 시리얼 모니터 사용
  Serial1.begin(9600);    // Bluetooth 센서 이용 18, 19 ( TX RX 반대로꽂기)
  Serial.println("Hello!");
 // BTSerial.begin(9600);
  pinMode(FAN_PIN, OUTPUT); // 환기구 팬
  pinMode(VCCSUP , OUTPUT); 
  digitalWrite(VCCSUP, HIGH);
  digitalWrite(FAN_PIN, LOW);

  // fingerprintsetup /////
  init_finger();

  ///////////////////////
}


int getFingerprintIDez() {  // 지문 인증 
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1; // 이미지가 인식되지 않을 경우 종료

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;  // 이미지가 변환되지 않으면 종료

  p = finger.fingerFastSearch();
  if (p==FINGERPRINT_NOTFOUND) {
    Serial.println("did not find a match"); // 맞는 지문이 없으면 종료
    return p;
  }
  if (p != FINGERPRINT_OK)  return -2;

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return 131; // 지문 인증 성공 시
}

void loop(){
  while (Serial1.available()){
    byte data = Serial1.read();
    Serial1.flush();
    if(data=='1'){    // 온습도 기준치 이상, FAN 작동
      FAN=1;
      Serial.println("1 received.\n");
      digitalWrite(FAN_PIN, HIGH);
      Serial.println("1 work finish\n");
    }
    else if(data=='2'){ // 온습도 기준치 미만, FAN 정지
      Serial.println("2 received.\n");
      digitalWrite(FAN_PIN, LOW);
      
      FAN=0;
    }
    else if(data=='3'){ // 비밀 곡 인증 완료, 지문 인식 시작
      Serial.println("3 received.\n");
      while(1){
        uint8_t result=getFingerprintIDez();
        delay(100);
        Serial1.write("n\n");
        if(result==131){
          Serial.println("Success.\n");
          Serial1.write("y\n");
          break;
        }
        else if(result==FINGERPRINT_NOTFOUND){
          Serial1.write("x\n");
          delay(3000);
        }
        if(Serial1.available()){
          byte d = Serial1.read();
          Serial1.flush();
          if(d=='c'){
            Serial.println("cancled.\n");
            break;
          }
        }
      }
    }
    else if(data=='4'){ // 지문 신규 등록
      Serial.println("4 received.\n");
      while(1){
        uint8_t result; // 지문등록결과
        delay(100);
        Serial1.write("n\n");
        Serial.println("n\n");
        result=getFingerprintEnroll();
        if(result==true){
          break;
        }
        else if(result==0x36){
          Serial.println("cancled.\n");
          break;
        }
        
      }
    }
  }
}
