int semu_cli_main(int argc, char **argv, semu_frame_callback frame_callback,
                  void *frame_context, semu_machine_input_poll_fn input_poll,
                  void *input_poll_context)
{
    run_arguments arguments;
    semu_error error;
    const char *command;
    semu_error_clear(&error);
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    command = argv[1];
    if (strcmp(command, "list") == 0 && argc == 2) {
        return command_list();
    }
    if (strcmp(command, "show-profile") == 0 && argc == 3) {
        return command_show(argv[2]);
    }
    if (!parse_options(argc, argv, 2, &arguments, &error)) {
        fprintf(stderr, "%s: %s\n", command, error.text);
        usage(stderr);
        return 2;
    }
    if (strcmp(command, "list-layers") == 0) {
        return command_layers(arguments.profile);
    }
    if (strcmp(command, "validate") == 0) {
        return command_validate(&arguments);
    }
    if (strcmp(command, "run") == 0) {
        return command_run(&arguments, frame_callback, frame_context,
                           input_poll, input_poll_context);
    }
    fprintf(stderr, "unknown command %s\n", command);
    usage(stderr);
    return 2;
}
