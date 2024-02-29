#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <cmath>
#include <fstream>
#include <vector>
#include <queue>
#include <condition_variable>

std::mutex mtx1; // Implementa mutex para evitar data race
std::mutex mtx2;
std::mutex mtx3;
std::mutex puntajeMtx; // Mutex para proteger el puntaje
std::shared_mutex sharedMtx; // Implementa shared_mutex para permitir lecturas simultáneas

class Circulo {
public:
    sf::CircleShape forma;
    float velocidadX;
    float velocidadY;

    Circulo(float radio, float startX, float startY, float velocidadX, float velocidadY)
        : velocidadX(velocidadX), velocidadY(velocidadY) {
        forma.setRadius(radio);
        forma.setFillColor(sf::Color::Red);
        forma.setPosition(startX, startY);
    }

    void mover() {
        std::unique_lock<std::mutex> lock1(mtx1, std::defer_lock); // Implementa try lock
        std::unique_lock<std::mutex> lock2(mtx2, std::defer_lock);
        std::unique_lock<std::mutex> lock3(mtx3, std::defer_lock);
        std::unique_lock<std::shared_mutex> lock(sharedMtx, std::defer_lock);

        std::lock(lock1, lock2, lock3, lock); // Implementa try lock

        forma.move(velocidadX, velocidadY);

        sf::Vector2f posicion = forma.getPosition();
        if (posicion.x < 0 || posicion.x > 800 || posicion.y < 0 || posicion.y > 600) {
            velocidadX = -velocidadX;
            velocidadY = -velocidadY;
        }
    }
};

// Función que maneja el movimiento de un círculo
void moverCirculo(Circulo& circulo) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        circulo.mover();
    }
}

// Función que ejecuta un círculo del pool de hilos
void ejecutarCirculo(std::shared_ptr<Circulo> circulo) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        circulo->mover();
    }
}

bool hayColision(const sf::CircleShape& circulo1, const sf::CircleShape& circulo2) {
    sf::Vector2f distancia = circulo1.getPosition() - circulo2.getPosition();
    std::lock_guard<std::mutex> lock(mtx1); // Implementa mutex para evitar data race

    float distanciaTotal = std::sqrt(distancia.x * distancia.x + distancia.y * distancia.y);
    return distanciaTotal < circulo1.getRadius() + circulo2.getRadius();
}

void guardarPuntaje(int puntaje) {
    std::ofstream archivo("puntaje.csv");
    archivo << "Puntaje" << std::endl;
    archivo << puntaje << std::endl;
}

int main() {
    sf::RenderWindow ventana(sf::VideoMode(800, 600), "juego con threads");
    sf::RectangleShape marco(sf::Vector2f(800, 600));
    marco.setFillColor(sf::Color::Blue);
    marco.setOutlineColor(sf::Color::Green);
    marco.setOutlineThickness(5.0f);

    Circulo circuloControlado(40.0f, 100.0f, 100.0f, 0.0f, 0.0f);
    Circulo circuloMovil1(20.0f, 300.0f, 300.0f, 1.5f, -1.5f);
    Circulo circuloMovil2(25.0f, 500.0f, 500.0f, -1.0f, -1.0f);
    Circulo circuloMovil3(30.0f, 400.0f, 400.0f, 2.0f, 2.0f);

    std::thread hiloCirculoControlado(moverCirculo, std::ref(circuloControlado));

    // Pool de hilos para los círculos móviles
    std::vector<std::thread> poolHilos;
    poolHilos.emplace_back(std::thread(ejecutarCirculo, std::make_shared<Circulo>(circuloMovil1)));
    poolHilos.emplace_back(std::thread(ejecutarCirculo, std::make_shared<Circulo>(circuloMovil2)));
    poolHilos.emplace_back(std::thread(ejecutarCirculo, std::make_shared<Circulo>(circuloMovil3)));

    std::chrono::steady_clock::time_point lastCollisionTime = std::chrono::steady_clock::now();
    int puntos = 0;
    circuloControlado.forma.setFillColor(sf::Color::Green);

    while (ventana.isOpen()) {
        sf::Event evento;
        while (ventana.pollEvent(evento)) {
            if (evento.type == sf::Event::Closed) {
                ventana.close();
            }

            if (evento.type == sf::Event::KeyPressed) {
                switch (evento.key.code) {
                case sf::Keyboard::W:
                    circuloControlado.velocidadY = -4.0f;
                    break;
                case sf::Keyboard::S:
                    circuloControlado.velocidadY = 4.0f;
                    break;
                case sf::Keyboard::A:
                    circuloControlado.velocidadX = -4.0f;
                    break;
                case sf::Keyboard::D:
                    circuloControlado.velocidadX = 4.0f;
                    break;
                }
            }

            if (evento.type == sf::Event::KeyReleased) {
                switch (evento.key.code) {
                case sf::Keyboard::W:
                case sf::Keyboard::S:
                    circuloControlado.velocidadY = 0.0f;
                    break;
                case sf::Keyboard::A:
                case sf::Keyboard::D:
                    circuloControlado.velocidadX = 0.0f;
                    break;
                }
            }
        }

        {
            std::shared_lock<std::shared_mutex> lock(sharedMtx); // Implementa shared_mutex para permitir lecturas simultáneas
            if (hayColision(circuloControlado.forma, circuloMovil1.forma) ||
                hayColision(circuloControlado.forma, circuloMovil2.forma) ||
                hayColision(circuloControlado.forma, circuloMovil3.forma)) {
                std::cout << "¡Colision! Juego terminado." << std::endl;
                guardarPuntaje(puntos); // Guardar puntaje en archivo csv
                ventana.close();
            }
            else {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastCollisionTime).count();
                if (elapsedTime >= 1) {
                    puntajeMtx.lock(); // Bloquear mutex para actualizar puntaje
                    puntos++;
                    puntajeMtx.unlock(); // Desbloquear mutex después de actualizar puntaje
                    lastCollisionTime = currentTime;
                    std::cout << "Puntos: " << puntos << std::endl;
                }
            }
        }

        circuloControlado.mover();

        // Mueve los círculos móviles
        circuloMovil1.mover();
        circuloMovil2.mover();
        circuloMovil3.mover();


        ventana.clear();
        ventana.draw(marco);
        ventana.draw(circuloControlado.forma);
        ventana.draw(circuloMovil1.forma);
        ventana.draw(circuloMovil2.forma);
        ventana.draw(circuloMovil3.forma);
        ventana.display();

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    hiloCirculoControlado.join();

    // Unirse a los hilos del pool
    for (auto& hilo : poolHilos) {
        hilo.join();
    }
    return 0;
}