import argparse
import sys
import time
from ortools.sat.python import cp_model

def read_graph(path):
    with open(path, 'r') as f:
        tokens = f.read().split()
    
    V = int(tokens[0])
    E = int(tokens[1])
    edges = [(int(tokens[i]), int(tokens[i+1])) for i in range(2, len(tokens), 2)]
    return V, E, edges

def solve_graph_iterative(V, edges, time_limit=60.0, threads=8):
    start_time_total = time.perf_counter()
    
    # Bucle iterativo idéntico al implementado en el motor CSP de C++
    for k in range(1, V + 1):
        print(f"> Probando con {k} colores...", file=sys.stderr, end="", flush=True)
        
        model = cp_model.CpModel()
        
        # Dominio estricto: [0, k-1]
        colors = [model.NewIntVar(0, k - 1, f"c{i}") for i in range(V)]
        
        for u, v in edges:
            model.Add(colors[u - 1] != colors[v - 1])
            
        # Rompimiento de simetría equivalente a fijar la primera asignación
        for i in range(min(k, V)):
            model.Add(colors[i] <= i)

        solver = cp_model.CpSolver()
        
        elapsed_so_far = time.perf_counter() - start_time_total
        remaining_time = time_limit - elapsed_so_far
        if remaining_time <= 0:
            print(" (Tiempo agotado)", file=sys.stderr)
            return -1, [], time_limit
            
        solver.parameters.max_time_in_seconds = remaining_time
        solver.parameters.num_search_workers = threads
        
        # Desactivamos el output del solver para mantener la consola limpia
        solver.parameters.log_search_progress = False
        
        status = solver.Solve(model)
        
        if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
            total_elapsed = time.perf_counter() - start_time_total
            print(f" ¡EXITO! ({total_elapsed:.3f} s)", file=sys.stderr)
            assignment = [solver.Value(colors[i]) for i in range(V)]
            return k, assignment, total_elapsed
        else:
            print(" (Falla)", file=sys.stderr)

    return -1, [], time.perf_counter() - start_time_total


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("graph", help="Archivo del grafo")
    parser.add_argument("--time", type=float, default=300.0, help="Límite de tiempo en segundos")
    parser.add_argument("--threads", type=int, default=8, help="Hilos para el solver")
    parser.add_argument("--out", type=str, default=None, help="Archivo de salida")
    args = parser.parse_args()

    V, E, edges = read_graph(args.graph)
    
    print(f"Iniciando OR-Tools (Iterativo) | Vértices: {V} | Aristas: {E}", file=sys.stderr)
    
    num_colors, assignment, dt = solve_graph_iterative(V, edges, args.time, args.threads)

    if num_colors == -1:
        print("\nTimeout: No se encontró solución factible.", file=sys.stderr)
        sys.exit(1)

    # Formato de salida exigido por la tarea: número de colores, seguido de color y vértice
    out_lines = [str(num_colors)]
    for v in range(V):
        out_lines.append(f"{assignment[v]} {v + 1}")
    
    output = "\n".join(out_lines) + "\n"
    
    if args.out:
        with open(args.out, "w") as f:
            f.write(output)
    else:
        sys.stdout.write(output)
        
    print(f"\nResumen: Colores={num_colors} | Tiempo total={dt:.3f}s", file=sys.stderr)


if __name__ == "__main__":
    main()
