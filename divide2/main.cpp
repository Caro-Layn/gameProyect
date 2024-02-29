#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <future>
#include <cmath>
#include <cstdlib>
#include <numeric>

const unsigned int NUMERO_CPUS = std::thread::hardware_concurrency() / 2;

template<typename T>
void fusionar(int izquierda, int medio, int derecha, std::vector<T>& arreglo) {
    int num_izquierda = medio - izquierda + 1;
    int num_derecha = derecha - medio;

    std::vector<T> arreglo_izquierda(arreglo.begin() + izquierda, arreglo.begin() + medio + 1);
    std::vector<T> arreglo_derecha(arreglo.begin() + medio + 1, arreglo.begin() + derecha + 1);

    int indice_izquierda = 0, indice_derecha = 0, indice_insertar = izquierda;

    while (indice_izquierda < num_izquierda && indice_derecha < num_derecha) {
        if (arreglo_izquierda[indice_izquierda] <= arreglo_derecha[indice_derecha]) {
            arreglo[indice_insertar] = arreglo_izquierda[indice_izquierda];
            ++indice_izquierda;
        }
        else {
            arreglo[indice_insertar] = arreglo_derecha[indice_derecha];
            ++indice_derecha;
        }
        ++indice_insertar;
    }

    while (indice_izquierda < num_izquierda) {
        arreglo[indice_insertar] = arreglo_izquierda[indice_izquierda];
        ++indice_izquierda;
        ++indice_insertar;
    }

    while (indice_derecha < num_derecha) {
        arreglo[indice_insertar] = arreglo_derecha[indice_derecha];
        ++indice_derecha;
        ++indice_insertar;
    }
}

template<typename T>
void ordenamiento_fusion_secuencial(int izquierda, int derecha, std::vector<T>& arreglo) {
    if (izquierda < derecha) {
        int medio = izquierda + (derecha - izquierda) / 2;
        ordenamiento_fusion_secuencial(izquierda, medio, arreglo);
        ordenamiento_fusion_secuencial(medio + 1, derecha, arreglo);
        fusionar(izquierda, medio, derecha, arreglo);
    }
}

template<typename T>
void ordenamiento_fusion_paralelo(int izquierda, int derecha, int profundidad, std::vector<T>& arreglo) {
    if (izquierda < derecha) {
        if (profundidad <= std::log2(NUMERO_CPUS)) {
            int medio = izquierda + (derecha - izquierda) / 2;
            auto future1 = std::async(std::launch::async, ordenamiento_fusion_paralelo<T>, izquierda, medio, profundidad + 1, std::ref(arreglo));
            ordenamiento_fusion_paralelo(medio + 1, derecha, profundidad + 1, arreglo);
            future1.wait();
            fusionar(izquierda, medio, derecha, arreglo);
        }
        else {
            ordenamiento_fusion_secuencial(izquierda, derecha, arreglo);
        }
    }
}

std::vector<int> leer_primera_columna_csv(const std::string& nombre_archivo, int limite = -1) {
    std::vector<int> primera_columna;
    std::ifstream archivo_csv(nombre_archivo);
    std::string linea;

    while (std::getline(archivo_csv, linea) && (limite == -1 || primera_columna.size() < static_cast<size_t>(limite))) {
        std::istringstream ss(linea);
        std::string valor;
        std::getline(ss, valor, ',');
        primera_columna.push_back(std::stoi(valor));
    }

    return primera_columna;
}

int main() {
    const std::string nombre_archivo = "valores.csv";
    std::vector<int> original_array = leer_primera_columna_csv(nombre_archivo, 1000000); // Limitado a 1048575 elementos
    int Elements = original_array.size();
    std::cout << "Se leyeron " << Elements << " elementos del archivo CSV" << std::endl;

    // Define el número de veces que se ejecutará cada implementación
    const int NUM_EVAL_RUNS = 10;
    std::vector<double> tiempos_secuenciales;
    std::vector<double> tiempos_paralelos;

    // Ejecución secuencial
    for (int i = 0; i < NUM_EVAL_RUNS; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<int> original_array_copy = original_array;
        ordenamiento_fusion_secuencial(0, Elements - 1, original_array_copy);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = end_time - start_time;
        tiempos_secuenciales.push_back(elapsed_seconds.count());
    }

    double tiempo_promedio_secuencial = std::accumulate(tiempos_secuenciales.begin(), tiempos_secuenciales.end(), 0.0) / NUM_EVAL_RUNS;

    // Ejecución paralela
    for (int i = 0; i < NUM_EVAL_RUNS; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<int> original_array_copy = original_array;
        ordenamiento_fusion_paralelo(0, Elements - 1, 0, original_array_copy);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = end_time - start_time;
        tiempos_paralelos.push_back(elapsed_seconds.count());
    }

    double tiempo_promedio_paralelo = std::accumulate(tiempos_paralelos.begin(), tiempos_paralelos.end(), 0.0) / NUM_EVAL_RUNS;

    // Calcular speedup y eficiencia
    double speedup = tiempo_promedio_secuencial / tiempo_promedio_paralelo;
    double eficiencia = (speedup / NUMERO_CPUS)*100;

    // Imprimir resultados
    std::cout << "Tiempo promedio de ejecución secuencial: " << tiempo_promedio_secuencial << " segundos" << std::endl;
    std::cout << "Tiempo promedio de ejecución paralela: " << tiempo_promedio_paralelo << " segundos" << std::endl;
    std::cout << "Speedup: " << speedup << std::endl;
    std::cout << "Eficiencia: " << eficiencia << std::endl;

    return 0;
}
