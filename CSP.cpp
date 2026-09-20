#include<bits/stdc++.h>

using namespace std;

using Variable = int;
using Value = int;
using Assignment = vector<Value>;

// =========================================================================== //
//  MODELADO GENERICO DEL CSP                                                   //
// =========================================================================== //
class Constraint{
    public:
        vector<Variable> scope;
        Constraint(vector<Variable> vars ) : scope(vars){}
        virtual ~Constraint() = default;
        virtual bool is_satisfied(const Assignment& assignment) const = 0;
        virtual int count_conflicts(const Assignment& assignment) const = 0;
};

class CSP{
    public:
        int num_variables;
        vector<Variable> variables;
        vector<vector<Value>> domains;
        vector<Constraint*> constraints;
        vector<vector<Constraint*>> constraint_index;
        vector<vector<Variable>> neighbors;

        CSP(int n, const vector<vector<Value>>& doms){
            num_variables = n;
            for(int i = 0; i < n; i++) variables.push_back(i);
            domains = doms;
            constraint_index.resize(n);
            neighbors.resize(n);
        }

        void add_constraint(Constraint* constraint){
            constraints.push_back(constraint);
            // Solo llenamos el indice aqui; los vecinos se construyen una vez al
            // final con build_neighbors() para evitar el O(grado^2) del find lineal.
            for(Variable v1 : constraint->scope){
                constraint_index[v1].push_back(constraint);
            }
        }

        // Construye la lista de vecinos de cada variable a partir del indice,
        // deduplicando con sort+unique -> O(sum grado log grado) en total.
        // Debe llamarse una vez, despues de agregar todas las restricciones.
        void build_neighbors(){
            for(int v = 0; v < num_variables; v++){
                neighbors[v].clear();
                for(Constraint* c : constraint_index[v])
                    for(Variable u : c->scope)
                        if(u != v) neighbors[v].push_back(u);
                sort(neighbors[v].begin(), neighbors[v].end());
                neighbors[v].erase(unique(neighbors[v].begin(), neighbors[v].end()),
                                   neighbors[v].end());
            }
        }

        bool is_consistent(Variable var, Value val, Assignment& assignment) const{
            assignment[var] = val;
            bool is_valid = true;
            for(Constraint* constraint: constraint_index[var]){
                if(!constraint->is_satisfied(assignment)){
                    is_valid = false;
                    break;
                }
            }
            assignment[var] = -1;
            return is_valid;
        }
};

class BinaryQueenConstraint : public Constraint {
    Variable v1, v2;
public:
    BinaryQueenConstraint(Variable a, Variable b) : Constraint({a, b}), v1(a), v2(b) {}

    bool is_satisfied(const Assignment& assignment) const override {
        if (assignment[v1] != -1 && assignment[v2] != -1) {
            Value val1 = assignment[v1];
            Value val2 = assignment[v2];
            if (val1 == val2 || abs(v1 - v2) == abs(val1 - val2)) {
                return false;
            }
        }
        return true;
    }

    int count_conflicts(const Assignment& assignment) const override{
        if(assignment[v1] != -1 && assignment[v2] != -1){
            Value val1 = assignment[v1];
            Value val2 = assignment[v2];
            if(val1 == val2 || abs(v1-v2) == abs(val1-val2)) return 1;
        }
        return 0;
    }
};

class GraphColoringConstraint : public Constraint{
    Variable v1, v2;
    public:
        GraphColoringConstraint(Variable a, Variable b) : Constraint({a, b}), v1(a), v2(b) {}

        bool is_satisfied(const Assignment& assignment) const override{
            if(assignment[v1] != -1 && assignment[v2] != -1){
                if(assignment[v1] == assignment[v2]){
                    return false;
                }
            }
            return true;
        }

        int count_conflicts(const Assignment& assignment) const override{
            if (assignment[v1] != -1 && assignment[v2] != -1) {
                if (assignment[v1] == assignment[v2]) return 1;
            }
            return 0;
        }
};

// =========================================================================== //
//  AC-3 / MRV / LCV / FORWARD CHECKING / BACKTRACKING                          //
// =========================================================================== //
bool revise(const CSP& csp, Variable i, Variable j, vector<vector<Value>>& local_domains) {
    bool revised = false;
    vector<Value> surviving;
    Assignment test_assignment(csp.num_variables, -1);

    for(Value x : local_domains[i]) {
        bool supported = false;
        test_assignment[i] = x;
        for(Value y : local_domains[j]) {
            test_assignment[j] = y;
            bool valid = true;
            for(Constraint* c : csp.constraint_index[i]) {
                if(find(c->scope.begin(), c->scope.end(), j) != c->scope.end()) {
                    if(!c->is_satisfied(test_assignment)) {
                        valid = false; break;
                    }
                }
            }
            if(valid) { supported = true; break; }
        }
        if(supported) surviving.push_back(x);
        else revised = true;
    }
    local_domains[i] = surviving;
    return revised;
}

bool ac3(const CSP& csp, vector<vector<Value>>& local_domains) {
    queue<pair<Variable, Variable>> q;
    for(Variable i = 0; i < csp.num_variables; i++) {
        for(Variable j : csp.neighbors[i]) q.push({i, j});
    }
    while(!q.empty()) {
        auto [i, j] = q.front(); q.pop();
        if(revise(csp, i, j, local_domains)) {
            if(local_domains[i].empty()) return false;
            for(Variable k : csp.neighbors[i]) {
                if(k != j) q.push({k, i});
            }
        }
    }
    return true;
}

Variable select_unassigned_variable_mrv(const CSP& csp, const Assignment& current_assignment, const vector<vector<Value>>& local_domains){
    Variable best_var = -1;
    size_t min_values = 999999;
    int max_degree = -1;

    for(Variable v : csp.variables){
        if(current_assignment[v] == -1){
            size_t valid_count = local_domains[v].size();

            int degree = 0;
            for(Variable neighbor : csp.neighbors[v]) {
                if(current_assignment[neighbor] == -1) degree++;
            }

            if(valid_count < min_values || (valid_count == min_values && degree > max_degree)){
                min_values = valid_count;
                max_degree = degree;
                best_var = v;
            }
        }
    }
    return best_var;
}

vector<Value> order_domain_values_lcv(const CSP& csp, Variable var, Assignment& current_assignment, const vector<vector<Value>>& local_domains) {
    // Nota: opera sobre current_assignment y restaura (is_consistent ya restaura),
    // por lo que no se copia la asignacion completa por cada valor.
    vector<pair<int, Value>> value_impact;
    for(Value val : local_domains[var]) {
        current_assignment[var] = val;
        int impact = 0;
        for(Variable neighbor : csp.neighbors[var]) {
            if(current_assignment[neighbor] == -1) {
                for(Value n_val : local_domains[neighbor]) {
                    if(!csp.is_consistent(neighbor, n_val, current_assignment)) {
                        impact++;
                    }
                }
            }
        }
        value_impact.push_back({impact, val});
    }
    current_assignment[var] = -1;
    sort(value_impact.begin(), value_impact.end());
    vector<Value> sorted_domain;
    for(auto& p : value_impact) sorted_domain.push_back(p.second);
    return sorted_domain;
}

bool forward_checking(const CSP& csp, Variable assigned_var, Assignment& current_assignment,
                      vector<vector<Value>>& local_domains,
                      vector<pair<Variable, Value>>& pruned_values){

    for(Variable neighbor : csp.neighbors[assigned_var]){
        if(current_assignment[neighbor] != -1) continue;

        vector<Value> surviving_values;
        for(Value test_value : local_domains[neighbor]){
            if(csp.is_consistent(neighbor, test_value, current_assignment)){
                surviving_values.push_back(test_value);
            } else {
                pruned_values.push_back({neighbor, test_value});
            }
        }
        local_domains[neighbor] = surviving_values;
        if(local_domains[neighbor].empty()){
            return false;
        }
    }
    return true;
}

bool backtrack(const CSP& csp, Assignment& current_assignment, vector<vector<Value>>& local_domains,
               int assigned_count, unsigned long long& iterations){
    iterations++;
    if(assigned_count == csp.num_variables) return true;

    Variable unassigned_var = select_unassigned_variable_mrv(csp, current_assignment, local_domains);
    vector<Value> domain_copy = order_domain_values_lcv(csp, unassigned_var, current_assignment, local_domains);

    for(Value val: domain_copy){
        current_assignment[unassigned_var] = val;
        vector<pair<Variable, Value>> pruned_values;

        if(forward_checking(csp, unassigned_var, current_assignment, local_domains, pruned_values)){
            if(backtrack(csp, current_assignment, local_domains, assigned_count + 1, iterations)) return true;
        }

        for(auto& p : pruned_values) local_domains[p.first].push_back(p.second);
        current_assignment[unassigned_var] = -1;
    }
    return false;
}

Assignment backtracking_search(const CSP& csp, unsigned long long& out_iterations){
    Assignment current_assignment(csp.num_variables, -1);
    vector<vector<Value>> local_domains = csp.domains;

    unsigned long long iterations = 0;

    // Si AC-3 vacia algun dominio, el CSP es inconsistente: no hay que buscar.
    if(!ac3(csp, local_domains)){
        out_iterations = 0;
        return {};
    }

    if(backtrack(csp, current_assignment, local_domains, 0, iterations)){
        out_iterations = iterations;
        return current_assignment;
    }
    out_iterations = iterations;
    return {};
}

// =========================================================================== //
//  ALGORITMO GENETICO HIBRIDO (mutacion min-conflicts INCREMENTAL)             //
// =========================================================================== //
class GeneticAlgorithm{
    private:
        int population_size;
        int max_generations;
        double mutation_rate;
        int max_stagnation;     // umbral para el reinicio por diversidad
        mt19937 rng;            // Mersenne Twister: aleatoriedad reproducible

        // Conflictos locales de la variable v con SUS vecinos, con el valor que
        // v tenga ahora mismo en 'individual'. O(grado de v), no O(#restricciones).
        int local_conflicts(const CSP& csp, const Assignment& individual, Variable v) const {
            int c = 0;
            for(Constraint* constraint : csp.constraint_index[v]){
                c += constraint->count_conflicts(individual);
            }
            return c;
        }

        Assignment tournament_selection(const vector<Assignment>& population, const vector<int>& fitnesses, int k = 3) {
            uniform_int_distribution<int> dist(0, population.size() - 1);
            int best_idx = dist(rng);
            for (int i = 1; i < k; i++) {
                int contender_idx = dist(rng);
                if (fitnesses[contender_idx] < fitnesses[best_idx]) {
                    best_idx = contender_idx;
                }
            }
            return population[best_idx];
        }

        pair<Assignment, Assignment> crossover(const Assignment& parent1, const Assignment& parent2) {
            int N = parent1.size();
            uniform_int_distribution<int> dist(0, N - 1);
            int cut_point = dist(rng);

            Assignment child1 = parent1;
            Assignment child2 = parent2;
            for (int i = cut_point; i < N; i++) {
                child1[i] = parent2[i];
                child2[i] = parent1[i];
            }
            return {child1, child2};
        }

        // Mutacion min-conflicts VERDADERAMENTE INCREMENTAL.
        // Recibe el fitness por referencia y lo actualiza por delta local: al mover
        // v de un valor a otro, solo cambian las restricciones que tocan a v, asi que
        //   fitness += (conflictos_locales_nuevos - conflictos_locales_viejos).
        // Nunca se recorre el total de restricciones.
        void mutate(Assignment& individual, int& fitness, const CSP& csp) {
            uniform_real_distribution<double> prob_dist(0.0, 1.0);

            for (Variable v : csp.variables) {
                if (prob_dist(rng) < mutation_rate) {
                    int old_local = local_conflicts(csp, individual, v);

                    int best_local = INT_MAX;
                    vector<Value> best_values;
                    for (Value val : csp.domains[v]) {
                        individual[v] = val;
                        int lc = local_conflicts(csp, individual, v);
                        if (lc < best_local) {
                            best_local = lc;
                            best_values.clear();
                            best_values.push_back(val);
                        } else if (lc == best_local) {
                            best_values.push_back(val);
                        }
                    }

                    // Desempate al azar para no sesgar el vecindario
                    uniform_int_distribution<int> val_dist(0, best_values.size() - 1);
                    individual[v] = best_values[val_dist(rng)];

                    // Actualizacion incremental del fitness global del individuo
                    fitness += (best_local - old_local);
                }
            }
        }

    public:
        GeneticAlgorithm(int pop_size = 100, int max_gen = 5000, double mut_rate = 0.2,
                         int stagnation = 40, unsigned int seed = 42)
            : population_size(pop_size), max_generations(max_gen), mutation_rate(mut_rate),
              max_stagnation(stagnation), rng(seed) {}

        // Fitness completo O(#restricciones): solo para individuos nuevos
        // (poblacion inicial y recien cruzados). La mutacion NO lo usa.
        int calculate_fitness(const CSP& csp, const Assignment& individual) const {
            int total_conflicts = 0;
            for(Constraint* c : csp.constraints){
                total_conflicts += c->count_conflicts(individual);
            }
            return total_conflicts;
        }

        Assignment create_random_individual(const CSP& csp){
            Assignment individual(csp.num_variables, -1);
            for(Variable v : csp.variables){
                uniform_int_distribution<int> dist(0, csp.domains[v].size() - 1);
                individual[v] = csp.domains[v][dist(rng)];
            }
            return individual;
        }

        // Devuelve la mejor asignacion y reporta en out_conflicts sus conflictos
        // (0 = solucion valida) y en out_generations la generacion donde termino.
        Assignment solve(const CSP& csp, int& out_conflicts, int& out_generations, bool verbose = true){
            vector<Assignment> population(population_size);
            vector<int> fitness(population_size);
            for(int i = 0; i < population_size; i++){
                population[i] = create_random_individual(csp);
                fitness[i] = calculate_fitness(csp, population[i]);
            }

            Assignment overall_best = population[0];
            int overall_best_fitness = fitness[0];
            for(int i = 1; i < population_size; i++){
                if(fitness[i] < overall_best_fitness){
                    overall_best_fitness = fitness[i];
                    overall_best = population[i];
                }
            }

            int stagnant_generations = 0;
            int restarts = 0;

            for(int gen = 0; gen < max_generations; gen++){
                // mejor de la generacion (sin recomputar: fitness ya esta al dia)
                int current_best_fitness = fitness[0];
                int current_best_idx = 0;
                for(int i = 1; i < population_size; i++){
                    if(fitness[i] < current_best_fitness){
                        current_best_fitness = fitness[i];
                        current_best_idx = i;
                    }
                }

                if(current_best_fitness < overall_best_fitness){
                    overall_best_fitness = current_best_fitness;
                    overall_best = population[current_best_idx];
                    stagnant_generations = 0;
                } else {
                    stagnant_generations++;
                }

                if(verbose && gen % 50 == 0){
                    cout << "  Gen: " << gen << " | Choques: " << overall_best_fitness
                         << " | Estancamiento: " << stagnant_generations << "/" << max_stagnation
                         << " | Reinicios: " << restarts << "   \r" << flush;
                }

                if(overall_best_fitness == 0){
                    if(verbose) cout << "\n  [GA] Solucion perfecta en generacion " << gen
                                     << " (reinicios: " << restarts << ")" << endl;
                    out_conflicts = 0;
                    out_generations = gen;
                    return overall_best;
                }

                vector<Assignment> new_population;
                vector<int> new_fitness;
                // Elitismo: sembramos SIEMPRE al mejor global (no solo al de esta gen)
                new_population.push_back(overall_best);
                new_fitness.push_back(overall_best_fitness);

                if(stagnant_generations >= max_stagnation){
                    // Reinicio catastrofico: reinyectamos diversidad (a la Doerr et al.)
                    while((int)new_population.size() < population_size){
                        Assignment ind = create_random_individual(csp);
                        new_population.push_back(ind);
                        new_fitness.push_back(calculate_fitness(csp, ind));
                    }
                    stagnant_generations = 0;
                    restarts++;
                } else {
                    while((int)new_population.size() < population_size){
                        Assignment parent1 = tournament_selection(population, fitness);
                        Assignment parent2 = tournament_selection(population, fitness);

                        auto [child1, child2] = crossover(parent1, parent2);

                        // Fitness completo una vez tras la cruza; luego la mutacion
                        // lo mantiene al dia de forma incremental.
                        int f1 = calculate_fitness(csp, child1);
                        mutate(child1, f1, csp);
                        new_population.push_back(child1);
                        new_fitness.push_back(f1);

                        if((int)new_population.size() < population_size){
                            int f2 = calculate_fitness(csp, child2);
                            mutate(child2, f2, csp);
                            new_population.push_back(child2);
                            new_fitness.push_back(f2);
                        }
                    }
                }
                population = move(new_population);
                fitness = move(new_fitness);
            }

            if(verbose) cout << "\n  [GA] Max generaciones. Mejor fitness: " << overall_best_fitness << endl;
            out_conflicts = overall_best_fitness;
            out_generations = max_generations;
            return overall_best;
        }
};

// =========================================================================== //
//  LECTURA DE GRAFOS                                                           //
// =========================================================================== //
struct Graph{
    int num_vertices;
    int num_edges;
    vector<pair<int, int>> edges;
};

Graph read_graph_from_file(const string& filename){
    ifstream file(filename);
    Graph g; g.num_vertices = 0; g.num_edges = 0;

    if(!file.is_open()){
        cerr << "Error al abrir el archivo " << filename << endl;
        return g;
    }
    file >> g.num_vertices >> g.num_edges;
    for(int i = 0; i < g.num_edges; i++){
        int u, v;
        file >> u >> v;
        g.edges.push_back({u, v});
    }
    file.close();
    return g;
}

// Verificacion independiente: cuenta conflictos totales de una asignacion completa
int total_conflicts(const CSP& csp, const Assignment& a){
    int c = 0;
    for(Constraint* con : csp.constraints) c += con->count_conflicts(a);
    return c;
}

// =========================================================================== //
//  RUNNERS (misma estructura para backtracking y genetico)                     //
// =========================================================================== //
enum class Method { BACKTRACKING, GENETIC };

void run_n_queens(int N, Method method){
    string mname = (method == Method::BACKTRACKING) ? "Backtracking (MRV+LCV+FC+AC3)" : "Algoritmo Genetico";
    cout << "\n=== N-Reinas | N=" << N << " | " << mname << " ===" << endl;

    vector<vector<Value>> domains(N);
    for(int i = 0; i < N; i++)
        for(int j = 0; j < N; j++)
            domains[i].push_back(j);

    CSP csp(N, domains);
    for(int i = 0; i < N; i++)
        for(int j = i + 1; j < N; j++)
            csp.add_constraint(new BinaryQueenConstraint(i, j));
    csp.build_neighbors();

    Assignment solution;
    auto start_time = chrono::high_resolution_clock::now();

    if(method == Method::BACKTRACKING){
        unsigned long long iters = 0;
        solution = backtracking_search(csp, iters);
        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> ms = end_time - start_time;
        if(solution.empty()) cout << "  Sin solucion." << endl;
        else cout << "  Solucion en " << ms.count() << " ms (nodos: " << iters
                  << ", conflictos: " << total_conflicts(csp, solution) << ")" << endl;
    } else {
        GeneticAlgorithm ga(/*pop*/100, /*gen*/5000, /*mut*/0.2, /*stagn*/40, /*seed*/42);
        int conflicts = -1, gens = -1;
        solution = ga.solve(csp, conflicts, gens);
        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> ms = end_time - start_time;
        cout << "  Terminado en " << ms.count() << " ms | conflictos: " << conflicts
             << " | verif: " << total_conflicts(csp, solution) << endl;
    }

    if(!solution.empty() && N <= 20 && total_conflicts(csp, solution) == 0){
        for(int i = 0; i < N; i++) cout << "  Reina fila " << i << " -> columna " << solution[i] << "\n";
    }

    for(Constraint* c : csp.constraints) delete c;
}

void run_graph_coloring(const string& filename, Method method){
    string mname = (method == Method::BACKTRACKING) ? "Backtracking" : "Algoritmo Genetico";
    cout << "\n=== Coloreado de grafo | " << filename << " | " << mname << " ===" << endl;

    Graph g = read_graph_from_file(filename);
    if(g.num_vertices == 0){ cout << "  Grafo vacio o no leido." << endl; return; }

    // Deteccion automatica del indexado: el archivo puede venir 0-indexado
    // (0..V-1) o 1-indexado (1..V). Usamos offset = id minimo y ajustamos el
    // numero de vertices por si el encabezado no coincide con los ids reales.
    int offset = 0, maxid = -1;
    if(g.num_edges > 0){
        offset = INT_MAX;
        for(auto& e : g.edges){
            offset = min(offset, min(e.first, e.second));
            maxid  = max(maxid,  max(e.first, e.second));
        }
    }
    int V = max(g.num_vertices, maxid - offset + 1);
    cout << "  Vertices: " << V << " | Aristas: " << g.num_edges
         << " | indexado desde " << offset << endl;

    for(int k = 1; k <= V; k++){
        cout << "  > Probando con " << k << " colores..." << flush;

        vector<vector<Value>> domains(V);
        for(int i = 0; i < V; i++)
            for(int c = 0; c < k; c++)
                domains[i].push_back(c);

        CSP csp(V, domains);
        for(auto& edge : g.edges)
            csp.add_constraint(new GraphColoringConstraint(edge.first - offset, edge.second - offset));
        csp.build_neighbors();

        Assignment solution;
        bool solved = false;
        auto start_time = chrono::high_resolution_clock::now();

        if(method == Method::BACKTRACKING){
            unsigned long long iters = 0;
            solution = backtracking_search(csp, iters);
            solved = !solution.empty();
            auto end_time = chrono::high_resolution_clock::now();
            chrono::duration<double, milli> ms = end_time - start_time;
            if(solved) cout << " OK en " << ms.count() << " ms (nodos: " << iters << ")" << endl;
            else cout << " falla" << endl;
        } else {
            GeneticAlgorithm ga(100, 5000, 0.2, 40, 42);
            int conflicts = -1, gens = -1;
            solution = ga.solve(csp, conflicts, gens, /*verbose*/false);
            solved = (conflicts == 0);
            auto end_time = chrono::high_resolution_clock::now();
            chrono::duration<double, milli> ms = end_time - start_time;
            if(solved) cout << " OK en " << ms.count() << " ms (gen: " << gens << ")" << endl;
            else cout << " falla (mejor conflictos: " << conflicts << ")" << endl;
        }

        if(solved){
            // Formato pedido: numero de colores, luego "color vertice"
            // (el vertice se reporta con el indexado original del archivo)
            unordered_set<int> usados(solution.begin(), solution.end());
            cout << "  Numero cromatico hallado: " << usados.size() << "\n";
            for(int i = 0; i < V; i++)
                cout << "  " << solution[i] << " " << (i + offset) << "\n";
            for(Constraint* c : csp.constraints) delete c;
            break;
        }
        for(Constraint* c : csp.constraints) delete c;
    }
}

int main(int argc, char** argv){
    // N-Reinas con ambos metodos
    run_n_queens(8, Method::BACKTRACKING);
    run_n_queens(8, Method::GENETIC);
    run_n_queens(30, Method::GENETIC);

    // Coloreado (descomenta y pasa tu archivo):
    // run_graph_coloring("grafo_50_nodos.txt", Method::BACKTRACKING);
    // run_graph_coloring("grafo_50_nodos.txt", Method::GENETIC);

    if(argc > 1){
        string file = argv[1];
        run_graph_coloring(file, Method::GENETIC);
    }
    return 0;
}
