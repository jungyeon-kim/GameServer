#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <windows.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <fstream>

using namespace std;
using namespace chrono;

#include "../../IOCPGameServer/IOCPGameServer/protocol.h"

sf::TcpSocket g_socket;

constexpr int SCREEN_WIDTH = SECTOR_WIDTH;
constexpr int SCREEN_HEIGHT = SECTOR_HEIGHT;

constexpr int TILE_WIDTH = 65;
constexpr int WINDOW_WIDTH = TILE_WIDTH * SCREEN_WIDTH / 2 + 10;   // size of window
constexpr int WINDOW_HEIGHT = TILE_WIDTH * SCREEN_WIDTH / 2 + 10;
constexpr int BUF_SIZE = 200;
constexpr int MAX_USER = NPC_ID_START;

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
	high_resolution_clock::time_point m_time_out, m_chat_timeOut{ high_resolution_clock::now() };
	sf::Text m_text, m_chat_text, m_chat_headText;
	sf::Text UI[3];
	sf::Text m_name, m_level, m_hp;

public:
	int PosX, PosY, Level, Exp, HP;
	bool isMoving{}, isAttacking{};
	bool isInputtingChat{};
	char Name[MAX_ID_LEN];
	sf::String m_input;
	high_resolution_clock::time_point move_timer{}, attack_timer{};

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

	void a_move(int NewPosX, int NewPosY) {
		m_sprite.setPosition((float)NewPosX, (float)NewPosY);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int NewPosX, int NewPosY) {
		PosX = NewPosX;
		PosY = NewPosY;
	}
	void update() {
		if (high_resolution_clock::now() - attack_timer > 0.5s) isAttacking = false;
		if (high_resolution_clock::now() - move_timer > 0.05s) isMoving = false;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (PosX - g_left_x) * 65.0f + 8;
		float ry = (PosY - g_top_y) * 65.0f + 8;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		m_name.setPosition(rx - 10, ry - 30);
		g_window->draw(m_name);
		m_level.setPosition(rx - 50, ry - 30);
		g_window->draw(m_level);
		m_hp.setPosition(rx - 10, ry - 70);
		g_window->draw(m_hp);

		// chat
		if (isInputtingChat)
		{
			g_window->draw(m_chat_headText);
			g_window->draw(m_chat_text);
		}
		else if (high_resolution_clock::now() < m_chat_timeOut)
		{
			m_chat_text.setPosition(rx - 10, ry - 120);
			g_window->draw(m_chat_text);
			if (m_input.getSize()) m_input.clear();
		}
		if (high_resolution_clock::now() < m_time_out) g_window->draw(m_text);
	}
	void draw_UI()
	{
		set_UI("Level: " + to_string(Level), 
			"Exp: " + to_string(Exp) + " / " + to_string(static_cast<int>(100 * pow(2, Level - 1))), 
			"HP: " + to_string(HP));

		for (int i = 0; i < 3; ++i) g_window->draw(UI[i]);
	}
	void set_name(char name[]) {
		m_name.setFont(g_font);
		m_name.setString(name);
		m_name.setFillColor(sf::Color(255, 255, 255));
		m_name.setStyle(sf::Text::Bold);
	}
	void set_level(string level) {
		m_level.setFont(g_font);
		m_level.setString(level);
		m_level.setFillColor(sf::Color(255, 255, 50));
		m_level.setStyle(sf::Text::Bold);
	}
	void set_hp(string hp) {
		m_hp.setFont(g_font);
		m_hp.setString(hp);
		m_hp.setFillColor(sf::Color(255, 0, 0));
		m_hp.setStyle(sf::Text::Bold);
	}
	void add_text(const char* chat) {
		m_text.setFont(g_font);
		m_text.setString(chat);
		m_text.setPosition(10, 1200);
		m_text.setFillColor(sf::Color(0, 50, 0));
		m_text.setStyle(sf::Text::Bold);
		m_text.setCharacterSize(50);
		m_time_out = high_resolution_clock::now() + 3s;
	}
	void add_chat_text(const char* chat) {
		m_chat_text.setFont(g_font);
		m_chat_text.setString(chat);
		m_chat_text.setPosition(170, WINDOW_HEIGHT * 2 - 70);
		m_chat_text.setFillColor(sf::Color(0, 0, 0));
		m_chat_text.setStyle(sf::Text::Bold);
		m_chat_text.setCharacterSize(50);
		m_chat_timeOut = high_resolution_clock::now() + 2s;
	}
	void add_chat_headText(const char* chat) {
		m_chat_headText.setFont(g_font);
		m_chat_headText.setString(chat);
		m_chat_headText.setPosition(10, WINDOW_HEIGHT * 2 - 70);
		m_chat_headText.setFillColor(sf::Color(20, 20, 20));
		m_chat_headText.setStyle(sf::Text::Bold);
		m_chat_headText.setCharacterSize(50);
	}
	void set_texture(sf::Texture& t, int PosX, int PosY, int x2, int y2) {
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(PosX, PosY, x2, y2));
	}
	void set_UI(string Level, string Exp, string HP) {
		for (int i = 0; i < 3; ++i)
		{
			UI[i].setFont(g_font);
			UI[i].setFillColor(sf::Color(0, 0, 255));
			UI[i].setStyle(sf::Text::Bold);
			UI[i].setCharacterSize(50);
		}
		UI[0].setString(Level);
		UI[0].setPosition(10, 50);
		UI[1].setString(Exp);
		UI[1].setPosition(10, 100);
		UI[2].setString(HP);
		UI[2].setPosition(10, 150);
	}
};

OBJECT avatar;
unordered_map <int, OBJECT> npcs;
int Tiles[WORLD_WIDTH][WORLD_HEIGHT]{};

OBJECT white_tile;
OBJECT black_tile;
OBJECT rock_tile;

sf::Texture* board;
sf::Texture* rock;
sf::Texture* player;
sf::Texture* wolf;

void client_initialize()
{
	ifstream In{ "TileData.txt", ios_base::binary };
	for (int i = 0; i < WORLD_WIDTH; ++i)
		for (int j = 0; j < WORLD_HEIGHT; ++j)
			Tiles[i][j] = In.get() - static_cast<char>('0');

	board = new sf::Texture;
	rock = new sf::Texture;
	player = new sf::Texture;
	wolf = new sf::Texture;
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		while (true);
	}
	board->loadFromFile("BG_Grass.png");
	rock->loadFromFile("Rock.png");
	player->loadFromFile("Player.png");
	wolf->loadFromFile("Wolf.png");
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	rock_tile = OBJECT{ *rock, 0, 0, TILE_WIDTH, TILE_WIDTH };
	avatar = OBJECT{ *player, 0, 0, 64, 64 };
	avatar.move(4, 4);
}

void client_finish()
{
	delete board;
	delete wolf;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_LOG:
	{
		SC_Packet_Log* my_packet = reinterpret_cast<SC_Packet_Log*>(ptr);
		if (my_packet->IsInGame) avatar.add_text(my_packet->Msg);
		else cout << my_packet->Msg << endl;
		break;
	}
	case SC_LOGIN_OK:
	{
		SC_Packet_Login_OK* my_packet = reinterpret_cast<SC_Packet_Login_OK*>(ptr);
		g_myid = my_packet->ID;
		avatar.move(my_packet->PosX, my_packet->PosY);
		avatar.Level = my_packet->Level;
		avatar.Exp = my_packet->Exp;
		avatar.HP = my_packet->HP;
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
				npcs[ID] = OBJECT{ *player, 64, 0, 64, 64 };
			else
				npcs[ID] = OBJECT{ *wolf, 0, 0, 64, 64 };
			strcpy_s(npcs[ID].Name, my_packet->Name);
			npcs[ID].set_name(my_packet->Name);
			npcs[ID].set_level(to_string(my_packet->Level));
			npcs[ID].set_hp(to_string(my_packet->HP));
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

		if (npcs.count(other_id)) npcs[other_id].add_chat_text(my_packet->Msg);
	}
	break;
	case SC_ATTACK_START:
	{
		SC_Packet_Attack* my_packet = reinterpret_cast<SC_Packet_Attack*>(ptr);
		int other_id{ my_packet->ID };

		npcs[other_id].set_texture(*player, 384, 0, 64, 64);
	}
	break;
	case SC_ATTACK_END:
	{
		SC_Packet_Attack* my_packet = reinterpret_cast<SC_Packet_Attack*>(ptr);
		int other_id{ my_packet->ID };

		npcs[other_id].set_texture(*player, 0, 0, 64, 64);
	}
	break;
	case SC_DEAD:
	{
		SC_Packet_DeadorAlive* my_packet = reinterpret_cast<SC_Packet_DeadorAlive*>(ptr);
		int other_id{ my_packet->ID };

		npcs[other_id].hide();
	}
	break;
	case SC_RESPAWN:
	{
		SC_Packet_DeadorAlive* my_packet = reinterpret_cast<SC_Packet_DeadorAlive*>(ptr);
		int other_id{ my_packet->ID };

		npcs[other_id].show();
	}
	break;
	case SC_LEVEL:
	{
		SC_Packet_Data* my_packet = reinterpret_cast<SC_Packet_Data*>(ptr);
		int ID{ my_packet->ID };
		int Data{ my_packet->Data };

		if (ID == g_myid) avatar.Level = Data;
		else npcs[ID].set_level(to_string(Data));
	}
	break;
	case SC_EXP:
	{
		SC_Packet_Data* my_packet = reinterpret_cast<SC_Packet_Data*>(ptr);
		int ID{ my_packet->ID };
		int Data{ my_packet->Data };

		if (ID == g_myid) avatar.Exp = Data;
	}
	break;
	case SC_HP:
	{
		SC_Packet_Data* my_packet = reinterpret_cast<SC_Packet_Data*>(ptr);
		int ID{ my_packet->ID };
		int Data{ my_packet->Data };

		if (ID == g_myid) avatar.HP = Data;
		else npcs[ID].set_hp(to_string(Data));
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
		wcout << L"서버 접속이 종료되었습니다." << endl;
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
	for(int i = avatar.PosX - 10; i < avatar.PosX + 9; ++i)
		for (int j = avatar.PosY - 10; j < avatar.PosY + 9; ++j)
		{
			if (i < 0 || j < 0) continue;

			if (Tiles[i][j] == 1)
			{
				rock_tile.a_move((i - g_left_x) * 65.0f + 8, (j - g_top_y) * 65.0f + 8);
				rock_tile.a_draw();
			}
		}
	

	avatar.update();
	avatar.draw_UI();
	avatar.draw();
	for (auto& npc : npcs) npc.second.draw();
	sf::Text text;
	text.setFont(g_font);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.PosX, avatar.PosY);
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

void send_attack_packet(char Type)
{
	CS_Packet_Attack m_packet;
	m_packet.Type = Type;
	m_packet.Size = sizeof(m_packet);
	send_packet(&m_packet);
}

void send_chat_packet(const char* msg)
{
	CS_Packet_Chat m_packet;
	m_packet.Type = CS_CHAT;
	m_packet.Size = sizeof(m_packet);
	strcpy_s(m_packet.Msg, msg);
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

	// 회원가입, 회원탈퇴, 로그인
	int Command{};
	char UserID[MAX_ID_LEN]{}, Password[MAX_ID_LEN]{};

	while (Command != 3)
	{
		cout << "-------------" << endl;
		cout << "[1] Sign Up" << endl;
		cout << "[2] Sign Out" << endl;
		cout << "[3] Login" << endl;
		cout << "-------------" << endl;

		cin >> Command;

		if (0 < Command && Command < 4)
		{
			cout << "input ID: ";
			cin >> UserID;
			cout << "input Password: ";
			cin >> Password;
		}

		switch (Command)
		{
		case 1:
		{
			CS_Packet_Login Packet;
			Packet.Size = sizeof(Packet);
			Packet.Type = CS_SIGNUP;
			memcpy(Packet.Name, UserID, sizeof(UserID));
			memcpy(Packet.Password, Password, sizeof(Password));
			send_packet(&Packet);

			char net_buf[BUF_SIZE];
			size_t	received;
			g_socket.setBlocking(true);
			auto recv_result = g_socket.receive(net_buf, BUF_SIZE, received);
			if (recv_result != sf::Socket::NotReady && received > 0) process_data(net_buf, received);
			g_socket.setBlocking(false);
			break;
		}
		case 2:
		{
			CS_Packet_Login Packet;
			Packet.Size = sizeof(Packet);
			Packet.Type = CS_SIGNOUT;
			memcpy(Packet.Name, UserID, sizeof(UserID));
			memcpy(Packet.Password, Password, sizeof(Password));
			send_packet(&Packet);

			char net_buf[BUF_SIZE];
			size_t	received;
			g_socket.setBlocking(true);
			auto recv_result = g_socket.receive(net_buf, BUF_SIZE, received);
			if (recv_result != sf::Socket::NotReady && received > 0) process_data(net_buf, received);
			g_socket.setBlocking(false);
			break;
		}
		case 3:
		{
			CS_Packet_Login Packet;
			Packet.Size = sizeof(Packet);
			Packet.Type = CS_LOGIN;
			memcpy(Packet.Name, UserID, sizeof(UserID));
			memcpy(Packet.Password, Password, sizeof(Password));
			strcpy_s(avatar.Name, Packet.Name);
			avatar.set_name(Packet.Name);
			send_packet(&Packet);
			break;
		}
		default:
			cout << "잘못된 명령어입니다." << endl;
			break;
		}
	}

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
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
			if (event.type == sf::Event::Closed) window.close();

			if (event.type == sf::Event::TextEntered && avatar.isInputtingChat)
			{
				if (event.key.code != sf::Keyboard::I) avatar.m_input += event.text.unicode;
				else if (avatar.m_input.getSize()) avatar.m_input.erase(avatar.m_input.getSize() - 1);

				avatar.add_chat_text(avatar.m_input.toAnsiString().c_str());
			}

			if (event.type == sf::Event::KeyReleased)
				if (event.key.code == sf::Keyboard::Space)
				{
					avatar.set_texture(*player, 0, 0, 64, 64);
					send_attack_packet(CS_ATTACK_END);
				}
			if (event.type == sf::Event::KeyPressed) {
				int p_type = -1;
				switch (event.key.code) {
				case sf::Keyboard::Enter:
				{
					if (!avatar.isInputtingChat)
					{
						avatar.isInputtingChat = true;
						avatar.add_chat_headText("CHAT: ");
					}
					else
					{
						avatar.isInputtingChat = false;
						send_chat_packet(avatar.m_input.toAnsiString().c_str());
					}

					break;
				}
				case sf::Keyboard::Left:
					if (!avatar.isMoving)
					{
						send_move_packet(D_LEFT);
						avatar.isMoving = true;
						avatar.move_timer = high_resolution_clock::now();
					}
					break;
				case sf::Keyboard::Right:
					if (!avatar.isMoving)
					{
						send_move_packet(D_RIGHT);
						avatar.isMoving = true;
						avatar.move_timer = high_resolution_clock::now();
					}
					break;
				case sf::Keyboard::Up:
					if(!avatar.isMoving)
					{
						send_move_packet(D_UP);
						avatar.isMoving = true;
						avatar.move_timer = high_resolution_clock::now();
					}
					break;
				case sf::Keyboard::Down:
					if (!avatar.isMoving)
					{
						send_move_packet(D_DOWN);
						avatar.isMoving = true;
						avatar.move_timer = high_resolution_clock::now();
					}
					break;
				case sf::Keyboard::Space:
					if (!avatar.isAttacking)
					{
						avatar.set_texture(*player, 384, 0, 64, 64);
						send_attack_packet(CS_ATTACK_START);
						avatar.isAttacking = true;
						avatar.attack_timer = high_resolution_clock::now();
					}
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