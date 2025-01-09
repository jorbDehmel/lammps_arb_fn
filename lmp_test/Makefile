.PHONY:	run
run:	../example_controller.out
	mpirun -n 1 \
		../example_controller.out \
		: -n 3 \
		lmp -mpicolor 123 -in test.lmp

../example_controller.out:
	$(MAKE) -C .. example_controller.out
