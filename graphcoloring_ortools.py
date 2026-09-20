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


def get_upper_bound(V, edges):
    deg = [0] * (V + 1)
    for u, v in edges:
        deg[u] += 1
        deg[v] += 1
    return min(V, max(deg[1:] + [0]) + 1)


def solve_graph(V, edges, time_limit=60.0, threads=8):
    ub = max(get_upper_bound(V, edges), 1)

    model = cp_model.CpModel()
    colors = [model.NewIntVar(0, ub - 1, f"c{i}") for i in range(V)]

    for u, v in edges:
        model.Add(colors[u - 1] != colors[v - 1])

    for i in range(V):
        model.Add(colors[i] <= i)

    max_color = model.NewIntVar(0, ub - 1, "max_color")
    model.AddMaxEquality(max_color, colors)
    model.Minimize(max_color)

    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = time_limit
    solver.parameters.num_search_workers = threads

    start_time = time.perf_counter()
    status = solver.Solve(model)
    elapsed = time.perf_counter() - start_time

    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        assignment = [solver.Value(colors[i]) for i in range(V)]
        num_colors = max(assignment) + 1
        return num_colors, assignment, elapsed, status == cp_model.OPTIMAL
    
    return -1, [], elapsed, False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("graph", help="Input graph file")
    parser.add_argument("--time", type=float, default=60.0)
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--out", type=str, default=None)
    args = parser.parse_args()

    V, E, edges = read_graph(args.graph)
    num_colors, assignment, dt, is_opt = solve_graph(V, edges, args.time, args.threads)

    if num_colors == -1:
        print("Timeout: No feasible solution found.", file=sys.stderr)
        sys.exit(1)

    out_lines = [str(num_colors)]
    for v in range(V):
        out_lines.append(f"{assignment[v]} {v + 1}")
    
    output = "\n".join(out_lines) + "\n"
    
    if args.out:
        with open(args.out, "w") as f:
            f.write(output)
    else:
        sys.stdout.write(output)
        
    print(f"[{args.graph}] V={V} E={E} Colors={num_colors} Optimal={is_opt} Time={dt:.3f}s", file=sys.stderr)


if __name__ == "__main__":
    main()