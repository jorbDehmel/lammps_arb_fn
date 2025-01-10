.PHONY:	run
run:	run1 run2 run3

.PHONY:	run1
run1:	../example_controller.out
	mpirun --map-by :OVERSUBSCRIBE -n 1 \
		../example_controller.out \
		: --map-by :OVERSUBSCRIBE -n 3 \
		lmp -mpicolor 123 -in test.lmp
	mv movie.avi $@.avi

.PHONY:	run2
run2:	../example_controller_2.py
	chmod +x ../example_controller_2.py
	mpirun --map-by :OVERSUBSCRIBE -n 1 \
		../example_controller_2.py \
		: --map-by :OVERSUBSCRIBE -n 3 \
		lmp -mpicolor 123 -in test.lmp
	mv movie.avi $@.avi

.PHONY:	run3
run3:	../example_bulk_controller.out
	mpirun --map-by :OVERSUBSCRIBE -n 1 \
		../example_bulk_controller.out \
		: --map-by :OVERSUBSCRIBE -n 3 \
		lmp -mpicolor 123 -in test.lmp
	mv movie.avi $@.avi

../%:
	$(MAKE) -C .. $($@:../%=%)
