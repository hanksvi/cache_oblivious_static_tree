#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>
using namespace std;



struct Node {
    int key;
    Node* left;
    Node* right;
    Node(int v) : key(v), left(nullptr), right(nullptr) {}
};

Node* bst_insert(Node* root, int key) {
    if (!root) return new Node(key);
    if (key < root->key) root->left  = bst_insert(root->left,  key);
    else  root->right = bst_insert(root->right, key);
    return root;
}


bool search_bst(Node* root, int key) {
    if (!root) return false;
    if (key == root->key) return true;
    if (key < root->key) return search_bst(root->left, key);
    return search_bst(root->right, key);
}

void free_bst(Node* root) {
    if (!root) return;
    free_bst(root->left);
    free_bst(root->right);
    delete root;
}


void build_veb(const vector<int>& sorted, int lo, int hi, vector<int>& out) {
    if (lo > hi) return;
    int mid = (lo + hi) / 2;
    out.push_back(sorted[mid]);
    build_veb(sorted, lo,    mid - 1, out);
    build_veb(sorted, mid + 1, hi,   out);
}


inline int left_size(int n) {
    return (n - 1) / 2;
}

bool search_veb(const vector<int>& v, int root, int n, int k) {
    while (n > 0) {
        if (k == v[root]) return true;
        int ls = left_size(n);
        if (k < v[root]) {
            root = root + 1;
            n    = ls;
        } else {
            root = root + 1 + ls;
            n    = n - ls - 1;
        }
    }
    return false;
}


// generador de datos + queries

vector<int> generate_sorted_data(int N, int range, mt19937& rng) {
    vector<int> all(range);
    iota(all.begin(), all.end(), 1);
    shuffle(all.begin(), all.end(), rng);
    all.resize(N);
    sort(all.begin(), all.end());
    return all;
}

// Q queries aleatorias (mezcla de presentes y ausentes)
vector<int> generate_queries(int Q, int range, mt19937& rng) {
    uniform_int_distribution<int> dist(1, range);
    vector<int> queries(Q);
    for (int& q : queries) q = dist(rng);
    return queries;
}


struct BenchResult {
    double bst_ms;
    double veb_ms;
};

BenchResult run_experiment(int N, int Q, mt19937& rng) {
    const int RANGE = N * 3;


    vector<int> sorted_data = generate_sorted_data(N, RANGE, rng);
    vector<int> queries     = generate_queries(Q, RANGE, rng);

    vector<int> insert_order = sorted_data;
    shuffle(insert_order.begin(), insert_order.end(), rng);
    Node* bst_root = nullptr;
    for (int x : insert_order)
        bst_root = bst_insert(bst_root, x);

    
    vector<int> veb;
    veb.reserve(N);
    build_veb(sorted_data, 0, (int)sorted_data.size() - 1, veb);
    int veb_n = (int)veb.size();

    
    volatile int bst_hits = 0;
    auto t0 = chrono::high_resolution_clock::now();
    for (int q : queries)
        if (search_bst(bst_root, q)) bst_hits++;
    auto t1 = chrono::high_resolution_clock::now();
    double bst_ms = chrono::duration<double, milli>(t1 - t0).count();

    
    volatile int veb_hits = 0;
    auto t2 = chrono::high_resolution_clock::now();
    for (int q : queries)
        if (search_veb(veb, 0, veb_n, q)) veb_hits++;
    auto t3 = chrono::high_resolution_clock::now();
    double veb_ms = chrono::duration<double, milli>(t3 - t2).count();

    if (bst_hits != veb_hits)
        cerr << "WARNING: hit counts differ! BST=" << bst_hits
             << " vEB=" << veb_hits << endl;

    free_bst(bst_root);
    return {bst_ms, veb_ms};
}

int main(int argc, char* argv[]) {
    int N = (argc > 1) ? atoi(argv[1]) : 1000000;
    int Q = (argc > 2) ? atoi(argv[2]) : 100000;
    int T = (argc > 3) ? atoi(argv[3]) : 5;


    cout << " Cache-Oblivious Static Tree vs BST Benchmark " << endl;
    cout << "N=" << N << "  Q=" << Q << "  T=" << T << endl;
    cout << "(BST construido con inserciones en orden aleatorio)" << endl;
    cout << endl;

    mt19937 rng(42);

    vector<double> bst_times, veb_times;
    bst_times.reserve(T);
    veb_times.reserve(T);

    for (int t = 0; t < T; t++) {
        cout << "Experimento " << (t+1) << "/" << T << " ... " << flush;
        BenchResult r = run_experiment(N, Q, rng);
        bst_times.push_back(r.bst_ms);
        veb_times.push_back(r.veb_ms);
        cout << "BST=" << r.bst_ms << "ms  vEB=" << r.veb_ms << "ms" << endl;
    }

    // Estadísticas
    auto avg = [](const vector<double>& v) {
        return accumulate(v.begin(), v.end(), 0.0) / (double)v.size();
    };
    auto stddev = [](const vector<double>& v, double m) {
        double s = 0;
        for (double x : v) s += (x - m) * (x - m);
        return sqrt(s / (double)v.size());
    };

    double bst_avg = avg(bst_times);
    double veb_avg = avg(veb_times);
    double bst_std = stddev(bst_times, bst_avg);
    double veb_std = stddev(veb_times, veb_avg);
    double speedup = bst_avg / veb_avg;

    cout << endl;
    cout << " Resultados: " << endl;
    cout << "BST promedio: " << bst_avg << " ms  (+/-" << bst_std << ")" << endl;
    cout << "vEB promedio: " << veb_avg << " ms  (+/-" << veb_std << ")" << endl;
    cout << "Speedup (BST/vEB): " << speedup << "x" << endl;

    // Guardar CSV
    ofstream csv("results.csv");
    csv << "experimento,bst_ms,veb_ms\n";
    for (int t = 0; t < T; t++)
        csv << (t+1) << "," << bst_times[t] << "," << veb_times[t] << "\n";
    csv << "PROMEDIO," << bst_avg << "," << veb_avg << "\n";
    csv << "STDDEV,"   << bst_std << "," << veb_std << "\n";
    csv.close();
    cout << "Resultados guardados en results.csv" << endl;

    return 0;
}