#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <windows.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
using namespace std;
using namespace chrono;

#include "../../IOCPGameServer/IOCPGameServer/protocol.h"

sf::TcpSocket g_socket;

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = TILE_WIDTH * SCREEN_WIDTH / 2 + 10;   // size of window
constexpr auto WINDOW_HEIGHT = TILE_WIDTH * SCREEN_WIDTH / 2 + 10;
constexpr auto BUF_SIZE = 200;
constexpr auto MAX_USER = NPC_ID_START;

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow* g_window;
sf::Font g_font;

class OBJECT {
private:
	bool m_showing;
	sf::Sprite m_sprite;

	char m_mess[MAX_STR_LEN];
	high_resolution_clock::time_point m_time_out;
	sf::Text m_text;
	sf::Text m_name;

public:
	int m_x, m_y;
	char Name[MAX_ID_LEN];
	OBJECT(sf::Texture& t, int PosX, int PosY, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(PosX, PosY, x2, y2));
		m_time_out = high_resolution_clock::now();
	}
	OBJECT() {
		m_showing = false;
		m_time_out = high_resolution_clock::now();
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int PosX, int PosY) {
		m_sprite.setPosition((float)PosX, (float)PosY);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int PosX, int PosY) {
		m_x = PosX;
		m_y = PosY;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * 65.0f + 8;
		float ry = (m_y - g_top_y) * 65.0f + 8;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		m_name.setPosition(rx - 10, ry - 10);
		g_window->draw(m_name);
		if (high_resolution_clock::now() < m_time_out) {
			m_text.setPosition(rx - 10, ry + 15);
			g_window->draw(m_text);
		}
	}
	void set_name(char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);
		m_name.setFillColor(sf::Color(255, 255, 0));
		m_name.setStyle(sf::Text::Bold);
	}
	void add_chat(char chat[]) {
		m_text.setFont(g_font);
		m_text.setString(chat);
		m_time_out = high_resolution_clock::now() + 1s;
	}
};

OBJECT avatar;
unordered_map <int, OBJECT> npcs;

OBJECT white_tile;
OBJECT black_tile;

sf::Texture* board;
sf::Texture* pieces;

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		while (true);
	}
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	avatar = OBJECT{ *pieces, 128, 0, 64, 64 };
	avatar.move(4, 4);
}

void client_finish()
{
	delete board;
	delete pieces;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_LOGIN_OK:
	{
		SC_Packet_Login_OK* my_packet = reinterpret_cast<SC_Packet_Login_OK*>(ptr);
		g_myid = my_packet->ID;
		avatar.move(my_packet->PosX, my_packet->PosY);
		g_left_x = my_packet->PosX - (SCREEN_WIDTH / 2);
		g_top_y = my_packet->PosY - (SCREEN_HEIGHT / 2);
		avatar.show();
	}
	break;

	case SC_ENTER:
	{
		SC_Packet_Enter* my_packet = reinterpret_cast<SC_Packet_Enter*>(ptr);
		int ID = my_packet->ID;

		if (ID == g_myid) {
			avatar.move(my_packet->PosX, my_packet->PosY);
			g_left_x = my_packet->PosX - (SCREEN_WIDTH / 2);
			g_top_y = my_packet->PosY - (SCREEN_HEIGHT / 2);
			avatar.show();
		}
		else {
			if (ID < NPC_ID_START)
				npcs[ID] = OBJECT{ *pieces, 64, 0, 64, 64 };
			else
				npcs[ID] = OBJECT{ *pieces, 0, 0, 64, 64 };
			strcpy_s(npcs[ID].Name, my_packet->Name);
			npcs[ID].set_name(my_packet->Name);
			npcs[ID].move(my_packet->PosX, my_packet->PosY);
			npcs[ID].show();
		}
	}
	break;
	case SC_MOVE:
	{
		SC_Packet_Move* my_packet = reinterpret_cast<SC_Packet_Move*>(ptr);
		int other_id = my_packet->ID;
		if (other_id == g_myid) {
			avatar.move(my_packet->PosX, my_packet->PosY);
			g_left_x = my_packet->PosX - (SCREEN_WIDTH / 2);
			g_top_y = my_packet->PosY - (SCREEN_HEIGHT / 2);
		}
		else {
			if (0 != npcs.count(other_id))
				npcs[other_id].move(my_packet->PosX, my_packet->PosY);
		}
	}
	break;

	case SC_LEAVE:
	{
		SC_Packet_Leave* my_packet = reinterpret_cast<SC_Packet_Leave*>(ptr);
		int other_id = my_packet->ID;

		if (other_id == g_myid) {
			avatar.hide();
		}
		else {
			if (0 != npcs.count(other_id))
				npcs[other_id].hide();
		}
	}
	break;
	case SC_CHAT:
	{
		SC_Packet_Chat* my_packet = reinterpret_cast<SC_Packet_Chat*>(ptr);
		int other_id{ my_packet->ID };

		if (npcs.count(other_id)) npcs[other_id].add_chat(my_packet->Msg);
	}
	break;
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);

	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = g_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv 에러!";
		while (true);
	}

	if (recv_result == sf::Socket::Disconnected)
	{
		wcout << L"서버 접속이 종료되었거나 유효하지 않은 ID입니다." << endl;
		system("pause");
		g_window->close();
	}

	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			//if (((tile_x + tile_y) % 2) == 0) {
			if (((tile_x / 3 + tile_y / 3) % 2) == 0) {
				white_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				white_tile.a_draw();
			}
			else
			{
				black_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				black_tile.a_draw();
			}
		}
	avatar.draw();
	//	for (auto &pl : players) pl.draw();
	for (auto& npc : npcs) npc.second.draw();
	sf::Text text;
	text.setFont(g_font);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x, avatar.m_y);
	text.setString(buf);
	g_window->draw(text);

}

void send_packet(void* packet)
{
	char* p = reinterpret_cast<char*>(packet);
	size_t sent;
	g_socket.send(p, p[0], sent);
}

void send_move_packet(unsigned char dir)
{
	CS_Packet_Move m_packet;
	m_packet.Type = CS_MOVE;
	m_packet.Size = sizeof(m_packet);
	m_packet.Dir = dir;
	send_packet(&m_packet);
}


int main()
{
	wcout.imbue(locale("korean"));
	sf::Socket::Status status = g_socket.connect("127.0.0.1", SERVER_PORT);
	g_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		while (true);
	}

	client_initialize();

	// ID 입력
	char UserID[MAX_ID_LEN]{};
	cout << "input ID: ";
	cin >> UserID;

	CS_Packet_Login l_packet;
	l_packet.Size = sizeof(l_packet);
	l_packet.Type = CS_LOGIN;
	memcpy(l_packet.Name, UserID, sizeof(UserID));
	//int t_id = GetCurrentProcessId();
	//sprintf_s(l_packet.Name, "P%03d", t_id % 1000);
	strcpy_s(avatar.Name, l_packet.Name);
	avatar.set_name(l_packet.Name);
	send_packet(&l_packet);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH , WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	sf::View view = g_window->getView();
	view.zoom(2.0f);
	view.move(SCREEN_WIDTH * TILE_WIDTH / 4, SCREEN_HEIGHT * TILE_WIDTH / 4);
	g_window->setView(view);

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyPressed) {
				int p_type = -1;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					send_move_packet(D_LEFT);
					break;
				case sf::Keyboard::Right:
					send_move_packet(D_RIGHT);
					break;
				case sf::Keyboard::Up:
					send_move_packet(D_UP);
					break;
				case sf::Keyboard::Down:
					send_move_packet(D_DOWN);
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}