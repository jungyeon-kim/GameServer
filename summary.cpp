/*
	동기:		수행이 순차적으로 행해지는 것				(수행 결과를 신경씀)	
	비동기:		수행이 비순차적으로 행해지는 것			(수행 결과를 신경 안씀)
	블락:		어떤 수행을 끝마칠 때까지 기다리는 것		(제어권 반환 x)
	논블락:		어떤 수행을 끝마칠 때까지 기다리지 않는 것	(제어권 반환 o)	-> Lock Free, Wait Free 등

	동기화:		실행 순서를 맞추는 행위

	IOCP 내부 자료구조)
	Device List:			소켓을 관리하는 리스트 (키와 소켓으로 구성)
	I/O Queue:				완료된 I/O의 정보를 담고있는 큐 (FIFO)
	Thread Pool:			스레드들이 대기하는 큐 (LIFO)
							스레드 생성이 아니라 실행중인 스레드를 스레드풀에 등록하는 것
	기타등등
*/

/*
	CreateIoCompletionPort					IOCP 커널 객체 생성
	(INVALID_HANDLE_VALUE, NULL, NULL, 0):	마지막 인자는 사용할 코어의 개수 (0: 존재하는 코어 개수만큼 사용)
	CreateIoCompletionPort					socket을 hIOCP에 연결
	(socket, hIOCP, key, 0):				세번째 인자는 소켓에 사용할 고유 키값 (네번째 인자는 무시)
	GetQueuedCompletionStatus():			1. 해당 스레드를 IOCP 스레드풀에 등록하고 Block
											2. 완료된 IO 작업의 결과를 넘겨줌
	PostQueuedCompletionStatus():			I/O Queue에 패킷을 추가
*/