# Socket programming FTP server
FTP(File Transfer Protocol) server를 구현하였습니다. socket programming을 이용하여 네트워크 통신을 구현합니다. client 프로그램과 server 프로그램으로 구성되어 있으며, client에서 사용자가 명령어를 입력하면 이를 server에 전송합니다. server는 명령어에 따라 수행합니다. server는 client의 연결 요청이 있을 때마다 프로세스를 생성하여 다중 접속을 허용하도록 합니다. 서버에 파일 업로드 및 다운로드, 디렉토리 내의 파일 종류 출력, 디렉토리 생성 및 삭제, 파일명 변경 등의 기능을 수행할 수 있습니다.


## result screen
![client](https://github.com/Choco-Coding/Socket_FTP_server/assets/117694927/a18ab5bd-55c8-45d1-8584-5a039fcebc4c)

client에서 FTP server에 접속한 화면입니다. 명령어를 입력하여 서버 내의 디렉토리 생성 및 삭제, 디렉토리 내의 파일 출력 등의 기능을 수행합니다.


![server](https://github.com/Choco-Coding/Socket_FTP_server/assets/117694927/b0114135-2cb4-4add-a735-0091f71f238d)

server에서 client와의 접속을 허용하고 받은 명령어를 수행합니다. 접속되어 있는 client의 이름, 접속 시간 등을 출력합니다.
