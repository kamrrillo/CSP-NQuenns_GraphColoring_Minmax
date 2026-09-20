#include<bits/stdc++.h>
#include<fstream> // Para manejar archivos CSV

using namespace std;

using Variable = int;
using Value = int;
using Assignment = vector<Value>; 

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
            for(Variable v1 : constraint->scope){
                constraint_index[v1].push_back(constraint);
                for(Variable v2 : constraint->scope){
                    if(v1 != v2){
                        if(find(neighbors[v1].begin(), neighbors[v1].end(), v2) == neighbors[v1].end()){
                            neighbors[v1].push_back(v2);
                        }
                    }
                }
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

    int count_conflicts(const Assignment& assignment) const override {
        if (assignment[v1] != -1 && assignment[v2] != -1) {
            Value val1 = assignment[v1];
            Value val2 = assignment[v2];
            if (val1 == val2 || abs(v1 - v2) == abs(val1 - val2)) {
                return 1;
            }
        }
        return 0;
    }
};

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

vector<Value> order_domain_values_lcv(const CSP& csp, Variable var, const Assignment& current_assignment, const vector<vector<Value>>& local_domains) {
    vector<pair<int, Value>> value_impact;
    for(Value val : local_domains[var]) {
        int impact = 0;
        Assignment temp_assign = current_assignment;
        temp_assign[var] = val;
        for(Variable neighbor : csp.neighbors[var]) {
            if(temp_assign[neighbor] == -1) {
                for(Value n_val : local_domains[neighbor]) {
                    if(!csp.is_consistent(neighbor, n_val, temp_assign)) {
                        impact++;
                    }
                }
            }
        }
        value_impact.push_back({impact, val});
    }
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
               int assigned_count, unsigned long long& iterations, int& max_depth){
    
    iterations++;
    if(assigned_count > max_depth) max_depth = assigned_count;

    // Solo imprimimos si el algoritmo se atora para saber que sigue vivo
    if (iterations % 1000000 == 0) {
        cout << "  [WARNING] Atorado... Iters: " << iterations 
             << " | Prof actual: " << assigned_count << "/" << csp.num_variables 
             << " | Max prof: " << max_depth << endl;
    }

    if(assigned_count == csp.num_variables){
        return true; 
    }  
    
    Variable unassigned_var = select_unassigned_variable_mrv(csp, current_assignment, local_domains);
    vector<Value> domain_copy = order_domain_values_lcv(csp, unassigned_var, current_assignment, local_domains);

    for(Value val: domain_copy){
        current_assignment[unassigned_var] = val;
        vector<pair<Variable, Value>> pruned_values;

        if(forward_checking(csp, unassigned_var, current_assignment, local_domains, pruned_values)){
            if(backtrack(csp, current_assignment, local_domains, assigned_count + 1, iterations, max_depth)){
                return true;
            }
        }
        
        for(auto& p : pruned_values){
            local_domains[p.first].push_back(p.second);
        }
        current_assignment[unassigned_var] = -1; 
    }
    return false;
}

// Actualizamos para recibir out_iterations por referencia
Assignment backtracking_search(const CSP& csp, unsigned long long& out_iterations){
    Assignment current_assignment(csp.num_variables, -1);
    vector<vector<Value>> local_domains = csp.domains;
    
    ac3(csp, local_domains); // Preprocesamiento silencioso
    
    unsigned long long iterations = 0;
    int max_depth = 0;
    
    if(backtrack(csp, current_assignment, local_domains, 0, iterations, max_depth)){
        out_iterations = iterations;
        return current_assignment;
    }
    
    out_iterations = iterations;
    return {}; 
}

int main(){
    // Abrir archivo CSV
    ofstream outfile("complejidad_backtracking.csv");
    outfile << "N,Tiempo_ms,Iteraciones\n";

    cout << "Iniciando recoleccion de datos para N-Reinas..." << endl;
    cout << "Los resultados se guardaran en 'complejidad_backtracking.csv'" << endl;
    cout << "Presiona Ctrl+C en cualquier momento para detener el programa.\n" << endl;

    // Llegar hasta N=150 es un buen limite inicial. 
    // Puedes subirlo si tu cluster sobrevive.
    int max_N = 250; 

    for(int N = 4; N <= max_N; N++){
        
        // 1. Preparar dominios
        vector<vector<Value>> domains(N);
        for(int i = 0; i < N; i++){
            for(int j = 0; j < N; j++){
                domains[i].push_back(j); 
            }
        }

        // 2. Instanciar CSP y restricciones
        CSP csp(N, domains);
        for(int i = 0; i < N; i++){
            for(int j = i + 1; j < N; j++){
                csp.add_constraint(new BinaryQueenConstraint(i, j));
            }
        }

        unsigned long long iteraciones = 0;
        
        // 3. Medir tiempo y ejecutar
        auto start_time = chrono::high_resolution_clock::now();
        Assignment solution = backtracking_search(csp, iteraciones);
        auto end_time = chrono::high_resolution_clock::now();
        
        chrono::duration<double, milli> ms_double = end_time - start_time;

        // 4. Guardar en CSV
        outfile << N << "," << ms_double.count() << "," << iteraciones << "\n";
        
        // 5. Imprimir en consola para ver el progreso
        cout << "N = " << N 
             << " | Tiempo: " << fixed << setprecision(2) << ms_double.count() << " ms" 
             << " | Iteraciones: " << iteraciones << endl;

        // 6. Limpiar memoria
        for (Constraint* c : csp.constraints) {
            delete c;
        }
    }

    outfile.close();
    cout << "\nEjecucion finalizada. Archivo cerrado exitosamente." << endl;
    return 0;
}
