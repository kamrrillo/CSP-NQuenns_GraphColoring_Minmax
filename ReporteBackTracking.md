# Reporte: evolución del solucionador CSP (backtracking + MRV + FC + AC-3)

Este documento traza cómo se construyó el solucionador de CSP para N-reinas, desde
la primera versión hasta la actual, pasando por los fallos detectados en cada
etapa y las correcciones que los resolvieron. La meta del código es un motor de
CSP genérico (clase `CSP` + `Constraint` abstracta) reutilizable después para
coloreado de grafos, con backtracking apoyado en las heurísticas y técnicas de
propagación estándar.

## Arquitectura general (constante en las tres versiones)

La estructura base fue acertada desde el inicio y se mantuvo: una clase
`Constraint` abstracta con un `scope` (las variables que involucra) y un método
`is_satisfied`, y una clase `CSP` que agrupa variables, dominios y restricciones.
Esta separación es la correcta porque permite cambiar de N-reinas a coloreado de
grafos escribiendo solo una nueva subclase de `Constraint`, sin tocar el motor de
búsqueda. Lo que cambió entre versiones no fue el diseño de alto nivel, sino las
estructuras de datos concretas y la lógica de propagación, que es donde estaban
los problemas de correctez y de eficiencia.

---

## Versión 1 — punto de partida

### Cómo estaba estructurada

- `Assignment` era un `unordered_map<Variable, Value>`.
- Las restricciones de N-reinas se modelaban con **una sola restricción global**,
  `NQueensConstraint`, cuyo `scope` eran las N variables. Su `is_satisfied`
  recorría todos los pares de variables asignadas comprobando ataque por columna y
  por diagonal.
- `is_consistent` **copiaba** la asignación completa antes de probar el valor:
  `Assignment local_assignment = assignment;`.
- `select_unassigned_variable_mrv` calculaba, para cada variable no asignada,
  cuántos valores eran consistentes llamando a `is_consistent` contra
  `csp.domains` (los dominios **completos**).
- `forward_checking` recorría todas las variables no asignadas y filtraba sus
  dominios con `is_consistent`.
- `backtrack` recibía la asignación y los dominios **por valor**, y dentro volvía
  a copiarlos (`new_assignment`, `new_domains`) en cada rama. El bucle iteraba
  sobre `csp.domains.at(unassigned_var)` (el dominio completo), comprobaba
  `is_consistent` y además corría forward checking.
- `revise`/`ac3` estaban implementados pero **no se invocaban**.

### Fallos detectados

**1. AC-3 conceptualmente incorrecto para restricciones no binarias.**
`revise` construía un `test_assignment` solo con `var_i` y llamaba
`is_consistent(var_j, y, ...)`, que a su vez recorría el `scope` global saltándose
todas las variables no asignadas. Con la restricción global de N-reinas esto
"funcionaba" por coincidencia (revisar dos reinas a la vez equivale al chequeo por
pares), pero no es válido en general: con una restricción global de verdad
(un `alldifferent`, o coloreado modelado como una sola restricción) AC-3 daría
resultados incorrectos. AC-3 necesita operar sobre **restricciones binarias**.

**2. MRV inconsistente con forward checking.**
`backtrack` ya recibía `local_domains` podados por FC, pero MRV los ignoraba y
recontaba consistencia contra los dominios completos. Es decir, MRV no contaba
sobre los dominios ya reducidos —que es justo lo que lo hace informativo— y encima
pagaba un costo alto (`O(vars × valores × constraints)`) en cada nodo.

**3. `is_consistent` costoso: copia + chequeo O(n²).**
Copiar el `unordered_map` completo en cada llamada, sumado a un `is_satisfied` que
revisaba **todos** los pares del scope cada vez, hacía cada verificación muy cara.
Invisible en N=8, letal en N=100.

**4. AC-3 nunca se usaba.**
Estaba escrito pero no se llamaba desde `backtracking_search`, y el enunciado lo
pide como parte de la entrega.

El diagnóstico global de esta versión: correcta para N=8 por el tamaño pequeño y
por la coincidencia del modelado, pero con un problema de correctez latente
(AC-3) y varios de eficiencia que impedirían escalar.

---

## Versión 2 — primera corrección

### Qué cambió

- Se introdujo `BinaryQueenConstraint`, una restricción **binaria** entre cada par
  de variables `(i, j)`. Con esto AC-3 pasó a ser correcto en principio, y el
  `main` crea una restricción por cada par de columnas.
- `is_consistent` dejó de copiar: ahora muta la asignación en el lugar
  (`assignment[var] = val; ... assignment.erase(var)`), probando y restaurando.
- MRV pasó a usar el tamaño de los `local_domains` ya podados (correcto).
- `backtrack` iteraba ya sobre los dominios podados en vez de los completos.
- Se agregó la liberación de memoria de las restricciones al final.

Ejemplo del cambio en `is_consistent` (de copiar a mutar):

```cpp
// v1
Assignment local_assignment = assignment;   // copia O(n)
local_assignment[var] = val;
// ... revisa constraints ...

// v2
assignment[var] = val;                        // sin copia
// ... revisa constraints ...
assignment.erase(var);                        // restaura
```

### Qué quedó pendiente

La corrección de correctez (restricciones binarias) fue el avance clave, pero
seguían los cuellos de eficiencia que impiden escalar:

**1. Lista plana de O(N²) restricciones recorrida en cada chequeo.**
`is_consistent` iteraba sobre **todas** las restricciones y por cada una hacía un
`find` lineal sobre su `scope` para ver si `var` participaba. Cada llamada quedaba
en O(N²); como forward checking hace O(N×N) llamadas y se ejecuta en cada nodo, un
solo nodo del árbol costaba del orden de O(N⁴) —inviable en N=100. La causa raíz:
`is_consistent` debía mirar solo las restricciones que contienen a `var`, no
todas.

**2. Mutar un `unordered_map` en el bucle más caliente.**
Mejor que copiar, pero insertar y borrar en un hash map (con hashing y posibles
rehashes) sigue siendo caro cuando las variables son simplemente `0..N-1`.

**3. Copias por nodo en el backtracking.**
`backtrack` recibía asignación y dominios por valor y volvía a copiar los dominios
(`new_domains`) en cada rama. Copiar el mapa de N vectores por nodo es carísimo a
N=100.

**4. Forward checking sobre todas las no asignadas.**
FC solo necesita filtrar los dominios de las variables **conectadas** a la recién
asignada, no de todas.

Se fijó un orden de prioridad para atacar estos puntos: primero cambiar
`Assignment` a `vector<int>`, luego el índice variable→restricciones, y luego el
backtracking con deshacer.

---

## Versión 3 — estado actual

### Qué se resolvió

**Assignment y dominios como vectores.**
`Assignment = vector<Value>` con `-1` como centinela de "no asignada", y
`domains` como `vector<vector<Value>>`. Acceso y prueba de asignación en O(1) real,
sin hashing. El chequeo de "¿está asignada?" pasó a ser `assignment[v] == -1`.

**Índice de adyacencia variable→restricciones.**
Se añadió `constraint_index : vector<vector<Constraint*>>`, poblado en
`add_constraint`. Con él, `is_consistent` solo recorre las restricciones donde
`var` participa, bajando de O(N²) a O(grado de la variable):

```cpp
// v3
bool is_consistent(Variable var, Value val, Assignment& assignment) const{
    assignment[var] = val;
    bool is_valid = true;
    for(Constraint* constraint : constraint_index[var]){   // solo las relevantes
        if(!constraint->is_satisfied(assignment)){ is_valid = false; break; }
    }
    assignment[var] = -1;
    return is_valid;
}
```

**Forward checking solo sobre vecinos.**
FC ahora reúne los vecinos no asignados a través del índice y solo poda sus
dominios, en vez de recorrer todas las variables.

**Backtracking con deshacer (undo), sin copiar dominios.**
`backtrack` pasa asignación y dominios por referencia. FC registra en
`pruned_values` cada `(vecino, valor)` que elimina; al retroceder, esos valores se
reinsertan y la variable se desasigna. Se acabó la copia de estructuras completas
por nodo:

```cpp
if(forward_checking(csp, unassigned_var, current_assignment, local_domains, pruned_values)){
    if(backtrack(...)) return true;
}
// undo
for(auto& p : pruned_values) local_domains[p.first].push_back(p.second);
current_assignment[unassigned_var] = -1;
```

### Verificación de la lógica

**El undo es correcto.** Cada valor podado se registra exactamente una vez por
rama (FC recorre cada vecino una sola vez por llamada), así que al reinsertar se
restaura exactamente lo que se quitó. El orden dentro del dominio cambia (los
valores restaurados quedan al final), pero eso solo altera el orden de
exploración, no la correctez.

**El `domain_copy` es necesario, por una razón sutil.** El bucle de `backtrack`
itera sobre una copia del dominio de la variable actual. FC en esta misma llamada
**no** toca ese dominio (la variable ya está asignada y no entra como vecino),
pero una llamada más profunda sí puede podarlo si esa variable es vecina de otra
más abajo. Iterar sobre la copia protege el bucle de esa mutación diferida.

**Detalle menor:** el valor asignado a una variable permanece en su dominio (no se
elimina al asignar). Es inofensivo aquí porque MRV la salta y FC de vecinos la
ignora, pero conviene tenerlo presente si luego se reutilizan dominios para contar
algo.

---

## Pendientes acordados

- **Reincorporar AC-3.** En la versión 3 se perdieron `revise`/`ac3`; hay que
  reescribirlos sobre la nueva estructura (dominios en `vector` + índice) porque el
  enunciado los pide. Nota importante: en N-reinas, AC-3 como preprocesamiento no
  poda nada al inicio (con dominios llenos toda columna tiene soporte), así que su
  efecto se aprecia mejor en coloreado, o usándolo como **MAC** (mantener
  arco-consistencia en cada nodo en lugar de forward checking).
- **Ordenamiento completo.** MRV está bien; falta el **degree heuristic** como
  desempate (barato con el índice) y mencionar **LCV** para el orden de valores,
  para cubrir el "ordenamiento" del enunciado.
- **Precomputar la lista de vecinos.** Construir un `unordered_set` de vecinos en
  cada llamada a FC tiene costo; conviene precomputar `vector<vector<Variable>>` de
  vecinos una sola vez en el CSP (sirve igual para coloreado).
- **Formato de salida para coloreado.** Respetar el formato pedido (primera línea:
  número de colores; luego: color seguido de vértice).

## Notas sobre escalamiento

Con la versión 3, N=100 (una solución) debería resolverse rápido. Dos advertencias
que no son bugs: (1) el backtracking cronológico puede atorarse en subárboles sin
solución para ciertas N —es la naturaleza del método, y ahí es donde min-conflicts
o una metaheurística resuelven N=100 y N=1000 casi al instante; (2) "encontrar
**todas** las soluciones de N=100" es inviable por enumeración exhaustiva (el número
de soluciones es descomunal), así que ese inciso hay que replantearlo.

Finalmente, un apunte de rumbo para la parte de metaheurística: el motor actual
trabaja sobre asignaciones **parciales** y consistencia, mientras que una
metaheurística (min-conflicts, recocido, genético) trabaja sobre asignaciones
**completas** y una función objetivo que cuenta violaciones. Son paradigmas
distintos: lo reutilizable entre ambos es la lógica de las restricciones
(`is_satisfied` como base para contar conflictos), no el andamiaje de
dominios/FC/AC-3.

---

## Resumen de la evolución

| Aspecto | v1 | v2 | v3 |
|---|---|---|---|
| Assignment | `unordered_map` | `unordered_map` | `vector<int>` (−1 = libre) |
| Restricciones N-reinas | una global O(n²) | binarias por par | binarias por par |
| `is_consistent` | copia + revisa todo | muta + revisa todo | muta + índice (O(grado)) |
| MRV | sobre dominios completos | sobre dominios podados | sobre dominios podados |
| Forward checking | todas las no asignadas | todas las no asignadas | solo vecinos |
| Backtracking | copia asignación y dominios | copia asignación y dominios | referencia + undo |
| AC-3 | incorrecto y sin usar | correcto pero sin usar | por reincorporar |
| Escala a N=100 | inviable | inviable | viable |
