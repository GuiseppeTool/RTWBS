# Documentation

Build the PDF:

```
pdflatex -interaction=nonstopmode -halt-on-error main.tex
bibtex main  # only if bibliography later added
pdflatex -interaction=nonstopmode -halt-on-error main.tex
pdflatex -interaction=nonstopmode -halt-on-error main.tex
```

Section overview:
1. DBM foundations
2. Time elapse operator
3. Zone graph construction
4. RTWBS theory
5. Game checking algorithm
6. Mapping theory <-> code
7. Future work

Minimal TeX dependencies: amsmath, amssymb, graphicx, xcolor, listings, cleveref, enumitem, tabularx, tikz, pgfplots, hyperref.
