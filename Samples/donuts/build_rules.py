#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Configuration file on how to build and clean projects in a specific folder.

This file is parsed by the cleanme, buildme, rebuildme and makeprojects
command line tools to clean, build and generate project files.
"""

# pylint: disable=global-statement
# pylint: disable=unused-argument

from __future__ import absolute_import, print_function, unicode_literals

# ``cleanme`` and ``buildme`` will process build_rules.py in the parent folder
# if True. Default is false
# Can be overridden above
CONTINUE = True

########################################


def configuration_settings(configuration):
    """
    Set up defines and libraries on a configuration basis.

    For each configation, set all configuration specific seting. Use
    configuration.name to determine which configuration is being processed.

    Args:
        configuration: Configuration class instance to update.

    Returns:
        None, to continue processing, zero is no error and stop processing,
        any other number is an error code.
    """

    configuration.define_list.append("USE_DSOUND")
    configuration.libraries_list.append("dsound.lib")

    # This doesn't exist in ARM 32 for Windows SDKs
    # Look in input.cpp for the data structures
    # configuration.libraries_list.append("dinput8.lib")

    # Open Watcom dinput.h doesn't support INITGUID,
    # so manually include the GUIDs as a library
    configuration.libraries_list.append("dxguid.lib")
