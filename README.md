# Business FX (fx-CG50)

Calculadora de referencia de **Business Management** para la **Casio fx-CG50** (add-in `.g3a`, C + gint/fxSDK). Bilingüe EN/ES: **F6** cambia el idioma en cualquier pantalla.

## Qué incluye
- **20 fórmulas** (contabilidad y mercado) con variables editables, procedimiento paso a paso con fracciones, respuesta con unidades y significado. Los valores de los campos admiten operaciones (`12*50`).
- **6 gráficos**: punto de equilibrio (ingresos, costos, costos fijos, PE, áreas de beneficio y pérdida), ingresos, costos totales, beneficio/pérdida, flujo de caja y crecimiento de ventas. Valores editables.
- **Diccionario** (49 términos, 7 categorías), **información** teórica (26 temas), **guía del plan de negocio** (7 partes) y **términos de instrucción** (8).
- **Búsqueda global** (ALPHA + tecla para letras), **favoritos**, **historial**, **calculadora científica** (sin, cos, tan, ln, log, sqrt, ^, pi, Ans, DEG/RAD).

## Compilar
Con fxSDK instalado: `fxsdk build-cg` → `build-cg/BusinessFX.g3a`. Cópialo a la calculadora por USB (modo almacenamiento).
Con GitHub: el workflow `.github/workflows/build.yml` compila y sube el `.g3a` como artefacto. La primera ejecución compila `sh-elf-gcc` (lenta, ~1 h); después usa caché.

## Límites (la pantalla no es una web)
- Sin KaTeX: las fórmulas se dibujan con barras de fracción y ASCII (`x`, `-`, `/`).
- "Copiar resultados" no existe en la calculadora: el botón **A Ans** envía el resultado a `Ans` de la calculadora científica.
- La fuente por defecto de gint es ASCII: las tildes y la ñ se muestran sin acento (`prep()` en `src/ui.c`).
- Favoritos e historial viven en RAM: se pierden al salir del add-in.

## Estado
La lógica (fórmulas, parser, formato, gráficos, menús) se probó en PC con cabeceras simuladas y ASAN/UBSAN. **No se compiló contra el SDK real**: si el CI falla por un nombre de tecla o función de gint, pega el error y se ajusta.
