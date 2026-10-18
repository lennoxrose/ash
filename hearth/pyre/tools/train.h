#ifndef PYRE_TRAIN_H
#define PYRE_TRAIN_H
// `pyre train` and `pyre check`: teaching and grading the manager on this machine.
// argv[0] is the program, argv[1] the command; flags follow (see `pyre help train`).
int cmd_train(int argc, char **argv);
int cmd_check(void);
#endif
