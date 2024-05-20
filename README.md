2021 - 2 IoT응용과실습 팀 프로젝트

팀명 : 판도라

팀장 : 권정우

팀원 : 정덕영

팀원 : 봉현진


주제 : 나만의 피아노 비밀금고

![KakaoTalk_20211215_113735110](https://github.com/BlackLair/PianoCoffer/assets/80610197/f12e4541-5272-499d-8d4c-331f52a04cbf)


비밀 멜로디를 연주하고 지문 인증을 해야 열 수 있는 나만의 피아노 비밀금고를 제작하였습니다.

비밀 멜로디와 지문은 잠금이 해제된 상태에서 변경할 수 있습니다. 비밀 멜로디의 경우 직접 녹음하여 건반을 누른 길이까지 저장됩니다. 사람이 비밀 멜로디를 연주할 때 milisecond 단위까지 똑같이 연주할 수 없기 때문에 허용할 수 있는 적당한 임계치를 설정하였습니다.

라즈베리파이, 3D 프린터와 다양한 센서나 모터와 같은 부품들을 활용하였습니다.

온습도를 측정하여 설정한 값 이상일 경우 환풍구가 자동으로 작동하는 기능을 구현하였습니다.

C언어(라즈베리파이, 아두이노)로 코딩하고, Autodesk Fusion 360을 이용해 피아노 건반을 모델링하여 3D 프린터를 통해 인쇄



시연 동영상 링크

외관
https://drive.google.com/file/d/10iutcgW0R8ForyC4our6i9kRKq_jjPc0/view?usp=drive_link

주요 기능
https://drive.google.com/file/d/1F5EI1GQPqf3rj1AVO_uuNozF0aXrd3wJ/view?usp=drive_link

도난방지기능
https://drive.google.com/file/d/1X26DPXjGWaVRqI_w3yZSqXGgsqibXa_u/view?usp=drive_link





bluetoothctl에서 scan on으로 기기 탐색
98:D3:31:F4:19:CB
pair [맥주소] 입력

trust [맥주소] 입력
명령창에 다음 입력
sudo rfcomm bind rfcommX [맥주소] <-라즈베리파이 재부팅마다 하
	->X에 숫자
	->소스코드에서 #define BLUE_PORT "/dev/rfcommX" 작성

소스코드
#define BAUD_RATE 9600 <-아두이노 Serial.begin(9600) 같게 설정

#gcc -o pandora pandora.c lcd.c -lwiringPi -lpthread

라즈베리파이 프로그램 먼저 실행 후 아두이노 리셋
