# Reporte — primera iteración del algoritmo genético

## Qué hice

Programé una primera versión de un GA para resolver los CSP de la tarea (N-reinas
y, con la idea de reusarlo, coloreado de grafos). Metí lo básico: población
inicial aleatoria, selección por torneo, cruza de un punto, mutación, y elitismo
guardando al mejor de cada generación. El fitness lo definí como el número total
de conflictos (para reinas, pares de reinas que se atacan; la meta es llegar a
fitness 0). El flujo por generaciones quedó funcionando y para N=8 encuentra
solución, así que al principio pensé que estaba bien. Cuando me puse a analizarlo
con calma me di cuenta de que tiene varios problemas, unos de diseño y otros de
eficiencia. Los anoto aquí porque son lo que quiero corregir en la siguiente
iteración.

## Lo que salió mal

El problema más serio no es un bug, es de diseño: **mi cruza pelea contra mi
representación**. Represento cada individuo como un valor por variable (la columna
de cada reina) y cruzo con un solo punto de corte, o sea, tomo la primera parte de
un padre y la segunda del otro. El detalle es que dos padres pueden tener pocos
conflictos cada uno y el hijo salir horrible, porque al cortar y pegar rompo las
relaciones diagonales que cada padre ya había acomodado. La cruza no está heredando
"bloques buenos", está mezclando a ciegas. Para reinas esto tiene arreglo: si uso
una representación de permutación (cada individuo es una permutación de columnas),
me ahorro de entrada los conflictos de fila y columna y solo peleo con las
diagonales. Pero entonces la cruza de un punto ya no sirve porque repite columnas;
tendría que usar algo como PMX u OX y mutación por intercambio. Para coloreado, en
cambio, mi representación actual sí sirve (un color por nodo, con repetición), así
que aquí tengo que decidir si hago dos representaciones o una sola y me aguanto
que en reinas la cruza sea mala.

Lo segundo, y creo que es lo que más me está costando resultados: **mi mutación es
puramente aleatoria**. Cuando muto una variable le pongo un valor cualquiera del
dominio. Leyendo un poco me quedó claro que en CSP la mutación que de verdad jala
es la de min-conflicts: al mutar una variable, en lugar de darle un valor al azar,
darle el que minimiza los conflictos de esa variable. Eso convierte al GA en algo
híbrido y es lo que hace que estos métodos resuelvan N=100 sin morir. Con mutación
aleatoria mi GA gasta un montón de generaciones o de plano se estanca.

Y hablando de estancarse: **no tengo ningún control de diversidad**. Con torneo más
elitismo y sin nada que mantenga variedad, la población colapsa rápido a copias del
mismo individuo, y cuando eso pasa la cruza deja de aportar (cruzar dos iguales da
lo mismo) y solo avanzo por mutación. Esto me conecta directo con el paper que me
tocó criticar (Doerr et al., 2024, el del (µ+1) GA), que justamente trata de por
qué mantener diversidad acelera al algoritmo de forma demostrable. Me parece buena
idea agregar algo simple —no permitir individuos repetidos, o reinicializar parte
de la población si el mejor no mejora en varias generaciones— y de paso comentar en
la crítica lo que vea pasar en mi propio código, en vez de hablar del paper en
abstracto.

Después están las cosas más de implementación. Uso `rand()` con `srand(time(0))`,
que además de ser un generador flojo me quita reproducibilidad y `rand() % dominio`
sesga un poco la distribución. Para poder comparar contra backtracking y OR-Tools
me conviene cambiar a `<random>` (mt19937 con distribución uniforme) y semilla
fija. No es lo más urgente pero es fácil y lo voy a necesitar para las mediciones.

De eficiencia, lo que me preocupa es que **recalculo el fitness completo de cada
individuo en cada generación**. Para reinas eso es recorrer todas las restricciones,
que son O(N²), por individuo, por toda la población, por cada generación. En N=100
con población de 100 son como 10⁶ operaciones por generación nada más en fitness, y
eso por miles de generaciones. Si me paso a mutación min-conflicts puedo calcular
los conflictos de forma incremental (solo lo que cambió al mutar), que debería ser
mucho más rápido.

Un par de cosas menores que noté revisando: mi condición de paro solo corta cuando
llego a fitness 0, así que si no encuentro solución me gasto las 1000 generaciones
completas aunque lleve rato sin mejorar; me conviene cortar también por
estancamiento (y eso mismo me sirve de gatillo para reinicializar por diversidad).
El elitismo lo dejé bien —al mejor lo meto tal cual y no lo paso por mutación— pero
tengo que asegurarme de que se mantenga intacto generación con generación. Y el
fitness depende de un `count_conflicts` que agregué a la clase Constraint; ahí tengo
que verificar que no cuente el mismo par dos veces y que fitness 0 corresponda
exactamente a una solución válida, tanto en reinas como en coloreado (aristas del
mismo color).

## Qué voy a cambiar

En orden de lo que creo que más impacto tiene: primero la mutación min-conflicts,
porque es el cambio con mayor retorno; luego resolver el tema de la representación
(me inclino por permutación para reinas y la genérica para coloreado, aunque
signifique dos cruzas distintas); después meter algún control de diversidad para no
converger antes de tiempo, que además me da material para la crítica del paper; y ya
al final el cambio a `<random>` y el fitness incremental.

Una conclusión honesta a la que llegué: para reinas y coloreado, min-conflicts o
recocido simulado a secas probablemente le ganen a un GA y son más simples. El
enunciado deja elegir. Si me quedo con el GA, la versión que de verdad compite es la
híbrida (mutación min-conflicts + diversidad), que es también la más interesante de
cara al paper de Doerr; un GA "de libro" con cruza de un punto y mutación aleatoria,
como el que tengo ahora, se queda corto en los tamaños grandes. Esta primera
iteración me sirvió sobre todo para ver eso.
