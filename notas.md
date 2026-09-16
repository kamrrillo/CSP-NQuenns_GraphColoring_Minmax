

# Reporte de Evolución Arquitectónica: Motor CSP (N-Reinas y Coloreado de Grafos)

Este documento detalla la transición del motor de Satisfacción de Restricciones (CSP) desde su concepción inicial como un modelo didáctico hasta su refactorización como un sistema de alto rendimiento capaz de escalar a problemas masivos ($N=100$).

---

## 1. La Lógica Original (Modelo Base)

El diseño inicial se estructuró siguiendo una arquitectura Orientada a Objetos estándar para facilitar la comprensión algorítmica. Las piezas clave eran:

*   **Estructuras de Datos Basadas en Hashing:** Se utilizó `std::unordered_map<Variable, Value>` para representar la asignación actual (el tablero) y `std::unordered_map<Variable, std::vector<Value>>` para los dominios. 
*   **Restricción Global Única:** Para el problema de N-Reinas, se implementó una sola clase `NQueensConstraint` que recibía todas las variables y, mediante un doble bucle `for`, verificaba iterativamente todos los pares de reinas asignadas para detectar ataques en columnas o diagonales.
*   **Búsqueda Fuerza Bruta con Copias:** La función de Backtracking dependía de realizar copias profundas del mapa de asignaciones y del mapa de dominios en cada nodo del árbol de recursión.
*   **Heurísticas Aisladas:** Se implementó *Forward Checking* para podar dominios futuros, y MRV (Minimum Remaining Values) para seleccionar la siguiente variable evaluando iterativamente la consistencia de cada valor posible.

---

## 2. Las Primeras Observaciones y Bugs Lógicos

Al ensamblar el primer prototipo funcional, se detectaron fallos críticos en la sintaxis y en el flujo de control que impedían la ejecución correcta.

### La Carga Lógica de los Errores
1.  **Retornos Prematuros (Bugs Silenciosos):** En funciones críticas como `is_consistent` y `select_unassigned_variable`, los retornos (`return true;` y `return -1;`) se colocaron *dentro* de los bucles `for`.
    *   *Consecuencia:* El motor solo verificaba la primera restricción de la lista o la primera variable disponible, ignorando el resto del espacio de búsqueda. Esto generaba falsos positivos masivos.
2.  **Incompletitud de la Interfaz:** Métodos declarados pero no definidos (como constructores) que impedían la compilación.

### Las Modificaciones Aplicadas
Se refactorizó el control de flujo moviendo los retornos fuera de los bucles iterativos. Se añadieron los constructores faltantes y se construyó un bloque `main` orquestador para inicializar las 8 variables, sus dominios (0-7), instanciar la restricción global y detonar la búsqueda. Con estos cambios, el algoritmo resolvió el tablero $N=8$ de manera exitosa.

---

## 3. El Análisis de Escalamiento (Nuevos Comentarios y Crítica Estructural)

Aunque el motor funcionaba para el "camino feliz" ($N=8$), una revisión profunda de la arquitectura reveló que el sistema estaba destinado a colapsar bajo su propio peso al intentar resolver $N=100$. Las observaciones detectaron tres fallas arquitectónicas severas:

### A. Correctez Matemática de AC-3
*   **El Problema:** El algoritmo AC-3 está diseñado matemáticamente para operar sobre redes de restricciones **binarias**. Al utilizar una única restricción global (`NQueensConstraint`), la función `revise` realizaba evaluaciones asimétricas y desperdiciaba ciclos evaluando variables no asignadas o restricciones irrelevantes. 
*   **La Realidad:** AC-3 funcionaba por pura coincidencia del modelo, pero fallaría catastróficamente al intentar usarlo con restricciones globales verdaderas (como `alldifferent`) o en el coloreado de grafos.

### B. El Cortocircuito entre MRV y Forward Checking
*   **El Problema:** La heurística MRV estaba recalculando la consistencia de los valores contra los dominios globales (`csp.domains`). 
*   **La Realidad:** Esto invalidaba y tiraba a la basura todo el trabajo de poda que ya había realizado el *Forward Checking* en los dominios locales. MRV estaba operando en $O(V \times D \times C)$ sin utilizar la señal real de los valores podados.

### C. La Tragedia del Rendimiento (Cuellos de botella $O(N^4)$)
Para escalar a $N=100$, el uso de memoria y tiempo del motor original era inviable por las siguientes razones:
1.  **Iteración Ciega de Restricciones:** `is_consistent` iteraba sobre *todas* las restricciones ($O(N^2)$ para reinas) y realizaba una búsqueda lineal (`find`) sobre sus *scopes*. 
2.  **Costo de la Poda:** `forward_checking` llamaba a `is_consistent` por cada variable no asignada y por cada valor, lo que convertía cada nodo del árbol en una operación de $O(N^4)$ (aproximadamente $10^8$ operaciones por nodo para $N=100$).
3.  **Abuso de Hashing y Copias:** Usar `unordered_map` para el tablero y realizar inserciones/borrados en el bucle más caliente del programa generaba un costo de *rehashing* altísimo. Además, copiar la estructura completa de dominios en cada llamada recursiva destruía la memoria caché.

---

## 4. El Plan de Refactorización Exigido

Para cumplir con los tiempos de cómputo requeridos por la práctica, se establecieron las siguientes directrices obligatorias de rediseño:

1.  **Cambio de Estructura Base:** Reemplazar el `unordered_map` del *Assignment* por un `std::vector<int>` con valores centinela (`-1`), garantizando acceso y mutación de $O(1)$ real.
2.  **Restricciones Estrictamente Binarias:** Sustituir la restricción global por una clase `BinaryQueenConstraint` que se instancie independientemente para cada par $(i, j)$.
3.  **Índice de Adyacencia O(1):** Implementar un mapa de `Variable -> vector<Constraint*>` en la clase `CSP`. Esto asegura que `is_consistent` y `forward_checking` solo evalúen los vecinos estrictamente conectados, bajando la complejidad dramáticamente.
4.  **Backtracking con Deshacer (Undo):** Eliminar las copias profundas de dominios. El algoritmo debe aplicar la poda, guardar un registro de qué valores eliminó, llamar a la recursión, y al regresar (backtrack), restaurar exactamente esos valores eliminados.
