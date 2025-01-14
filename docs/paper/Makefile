SECTIONS = sections/abstract.tex sections/titlepage.tex

main.pdf:	main.tex refs.bib $(SECTIONS)
	pdflatex -shell-escape $<
	# bibtex main.aux
	pdflatex -shell-escape $<
	pdflatex -shell-escape $<

.PHONY:	clean
clean:
	find . \( -iname "*.aux" -or -iname "*.pdf"  \) \
		-exec rm -f "{}" \;
