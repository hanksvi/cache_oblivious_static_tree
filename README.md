# Cache-Oblivious Static Tree vs BST

Implementación y benchmark de un **cache-oblivious static tree** (layout van Emde Boas implícito) comparado contra un BST con punteros. El objetivo es mostrar empíricamente la ventaja de cache del static tree para búsquedas.

---

## Estructura del proyecto

```
.
├── static_tree_benchmark.cpp   # Código completo (Etapas 1–5)
├── results.csv                 # Resultados del último benchmark
└── README.md
```

---

## Las 5 etapas

### Etapa 1 — BST con punteros

```cpp
struct Node { int key; Node* left; Node* right; };
```

El BST se construye insertando los elementos en **orden aleatorio** (shuffle antes de insertar). Esto es fundamental: si se insertara en orden ordenado produciría un árbol degenerado (lista enlazada). El orden aleatorio genera un árbol balanceado en expectativa, pero con nodos dispersos en memoria → muchos cache misses.

La búsqueda es recursiva:

```cpp
bool search_bst(Node* root, int key) {
    if (!root) return false;
    if (key == root->key) return true;
    if (key < root->key) return search_bst(root->left, key);
    return search_bst(root->right, key);
}
```

---

### Etapa 2 — vEB layout builder

```cpp
void build_veb(const vector<int>& sorted, int lo, int hi, vector<int>& out);
```

Toma el arreglo ordenado y lo almacena en **pre-orden del árbol implícito**: primero la raíz del subarreglo `[lo..hi]` (el elemento del medio), luego recursivamente el subárbol izquierdo, luego el derecho.

```
Arreglo sorted [1..7]:
           4
         /   \
        2     6
       / \   / \
      1   3 5   7

vEB layout: [4, 2, 1, 3, 6, 5, 7]
```

Este orden garantiza que los nodos de un subárbol de altura `h` ocupan `2^h - 1` posiciones **contiguas** en memoria, lo que minimiza cache misses en búsquedas.

---

### Etapa 3 — Búsqueda en static tree (sin punteros)

La navegación usa aritmética de índices. Para un subárbol de `n` nodos con raíz en `root`:

- El subárbol **izquierdo** tiene `(n-1)/2` nodos y empieza en `root + 1`
- El subárbol **derecho** tiene `n - (n-1)/2 - 1` nodos y empieza en `root + 1 + (n-1)/2`

```cpp
inline int left_size(int n) { return (n - 1) / 2; }

bool search_veb(const vector<int>& v, int root, int n, int k) {
    while (n > 0) {
        if (k == v[root]) return true;
        int ls = left_size(n);
        if (k < v[root]) { root = root + 1;        n = ls;          }
        else             { root = root + 1 + ls;   n = n - ls - 1;  }
    }
    return false;
}
```


---

### Etapa 4 — Generador de datos y queries

```cpp
// N enteros únicos ordenados en [1, 3N]
vector<int> generate_sorted_data(int N, int range, mt19937& rng);

// Q queries aleatorias en [1, 3N] (~1/3 serán hits)
vector<int> generate_queries(int Q, int range, mt19937& rng);
```

Se usa el rango `[1, 3N]` para que aproximadamente un tercio de las queries encuentren el elemento (hits) y dos tercios no (misses), simulando un workload realista.

---

### Etapa 5 — Benchmark y reporte

Se ejecutan `T` experimentos independientes. Cada experimento:
1. Genera un nuevo conjunto de datos y queries
2. Construye ambas estructuras
3. Mide el tiempo de `Q` búsquedas en BST y en static tree por separado
4. Verifica que ambas den el mismo número de hits (correctness check)

Resultados guardados en `results.csv`:

```
experimento,bst_ms,veb_ms
1,106.074,29.859
2,88.0366,25.2924
3,97.6529,24.1811
4,100.115,24.8373
5,93.2933,22.6815
PROMEDIO,97.0342,25.3703
STDDEV,6.10796,2.41174
```

---

## Resultados

Ejecutado con **N=1,000,000 elementos**, **Q=100,000 queries**, **T=5 experimentos**, búsqueda en BST **recursiva**:

| Estructura     | Tiempo promedio | Desviación std |
|----------------|-----------------|----------------|
| BST (punteros) | 97.03 ms        | +/-6.11 ms     |
| Static tree    | 25.37 ms        | +/-2.41 ms     |

**Speedup: ~3.83x** a favor del static tree.

La mayor desviación estándar del BST recursivo (6.11 ms vs 0.78 ms en la versión iterativa) refleja la variabilidad introducida por el overhead del stack en cada llamada recursiva.

### ¿Por qué es más rápido el static tree?

El BST con punteros tiene sus nodos dispersos en el heap (cada `new Node()` puede caer en cualquier página de memoria). Una búsqueda de longitud `O(log N)` puede tocar hasta `O(log N)` páginas distintas → muchos cache misses.

El static tree almacena los nodos en un arreglo contiguo con layout vEB. Para un árbol de altura `h`, los nodos de cualquier subárbol de altura `h/2` están **contiguos** en el arreglo. Si el bloque de cache tiene tamaño `B`, se necesitan `O(log_B N)` cache misses en vez de `O(log N)`.

---

## Compilar y ejecutar

```bash
g++ static_tree_benchmark.cpp
```

Esto genera `a.out` (Linux/Mac) o `a.exe` (Windows) con los parámetros por defecto: N=1,000,000 elementos, Q=100,000 queries, T=5 experimentos.

Si quieres cambiar los parámetros o el nombre del ejecutable:

```bash
g++ -O2 -o bench static_tree_benchmark.cpp
./bench 1000000 100000 5   # N  Q  T
```

---

## Notas de implementación

- El BST inserta en orden aleatorio (shuffle) para producir un árbol balanceado con layout realista en memoria. Insertar en orden ordenado crea una lista enlazada (O(N) altura) que sería una comparación injusta en el otro sentido.
- La búsqueda en BST es **recursiva**, lo que introduce overhead de stack frames (~20 niveles para N=1M) y aumenta la variabilidad en los tiempos.
- Se usa `volatile int hits` para evitar que el compilador elimine el bucle de búsqueda como código muerto.