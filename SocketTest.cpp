// SocketTest.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <iostream>
#include <clocale>

#include <SFML/Network.hpp>

#include <thread>
#include <mutex>

//Класс для работы с консолью из многопоточки
class Console {
	std::recursive_mutex mutex;
public:
	void lock() {
		mutex.lock();
	}

	void unlock() {
		std::cout.flush();
		mutex.unlock();
	}

	template<class T>
	auto& operator << (const T& o) {
		lock();
		std::cout << o;
		unlock();
		return *this;
	}
} console;


/// <summary>
/// Класс отвечающий за управления потоками
/// Поток можно запустить методом start, остановить методом stop
/// Можно уйти в бесконечный цикл из текущего потока вызвав loop
/// Содержит два чистых виртуальных метода init() и frame()
/// Первый вызывается при старте потока, второй в бесконечном цикле
/// до завершения выполнения потока
/// </summary>
class Thread {
	std::unique_ptr<std::thread> m_thread;
	std::atomic<bool> m_continue;

	static void _loop(Thread* thread) {
		thread->loop();
	}
protected:
	/// <summary>
	/// Метод вызывается в методе loop до цикла потока
	/// </summary>
	virtual void init() = 0;
	/// <summary>
	/// Метод взывается в цикле потока
	/// </summary>
	virtual void frame() = 0;
public:
	/// <summary>
	/// Запустить поток
	/// </summary>
	void start() {
		m_continue.store(true);
		m_thread = std::make_unique<std::thread>(_loop, this);
	}

	/// <summary>
	/// Остановить поток
	/// </summary>
	void stop() {
		m_continue.store(false);
		if (m_thread) { //Если поток вообще существует, то отключаем его и разрушаем объект
			m_thread->join();
			m_thread.reset();
		}
	}

	/// <summary>
	/// Цикл потока, выполняется в потоке после метода start
	/// Вызов приведёт к запуску бесконечного цикла, который можно остановить методом stop
	/// </summary>
	void loop() {
		m_continue.store(true);
		init();
		while (m_continue)
			frame();
	}
};

/// <summary>
/// Базовый класс для работы с передачей данных по протоколу tcp
/// Содержит протектид поля socket и packet для удобства
/// </summary>
class TcpThread : public Thread {
protected:
	sf::TcpSocket m_socket;
	sf::Packet m_packet;

	sf::IpAddress m_address;
	unsigned short m_port;
public:
	TcpThread(sf::IpAddress address, unsigned short port) :
		m_address(address), m_port(port) {
		m_socket.setBlocking(true);
	}
};


/// <summary>
/// Класс используется для считывания данных с консоли и отправки их по определённому адресу
/// </summary>
class TcpSender final : public TcpThread {
	std::string m_input;

	virtual void frame() override {
		console << "Введите данные\n";
		std::cin >> m_input;

		m_packet << m_input;

		console.lock();
		m_socket.send(m_packet);
		std::cout << "Данные отправлены\n";
		console.unlock();

		m_packet.clear();
	}

	virtual void init() override {
		console.lock();
		std::cout << "Подключение сокета (" << m_address << ":" << m_port << ")\n";
		console.unlock();

		if (m_socket.connect(m_address, m_port) == sf::Socket::Done) {
			console.lock();
			std::cout << "Подключен сокет (" << m_address << ":" << m_port << ")\n";
			console.unlock();
		} else {
			console << "Превышено время ожидания\n";
			stop();
		}
	}
public: using TcpThread::TcpThread;
};

/// <summary>
/// Класс ожидает данные, в случае их получения выводит на экран
/// </summary>
class TcpListener final : public TcpThread {
	std::string m_input;
	sf::TcpListener m_listener;

	virtual void frame() override {
		m_packet.clear();

		//Бесконечно пытаемся получить данные
		while (m_socket.receive(m_packet) != sf::Socket::Done) {}
		m_packet >> m_input;

		console.lock();
		std::cout << "Данные получены:\n";
		std::cout << "\t'" + m_input + "'\n";
		console.unlock();
	}

	
	//Прослушивает порт и выводит
	virtual void init() final override {
		console.lock();
		std::cout << "Прослушивание порта (" << m_port << ")\n";
		console.unlock();

		m_listener.listen(m_port);
		if (m_listener.accept(m_socket) == sf::Socket::Done) {
			console.lock();
			std::cout << "Подключение из адреса (" << m_socket.getRemoteAddress() << ":" << m_port << ")\n";
			console.unlock();
		} else {
			console << "Превышено время ожидания\n";
			stop();
		}
	}

public: using TcpThread::TcpThread;
};

int main() {
	std::setlocale(LC_ALL, "RUS");

	TcpSender sender(sf::IpAddress(127, 0, 0, 1), 4900);
	TcpListener listener(sf::IpAddress(127, 0, 0, 1), 4900);

	listener.start();
	sender.loop();
}