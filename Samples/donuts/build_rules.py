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

# List of projects to generate if makeprojects is invoked
# without any parameters, default create recommended
# project for the host machine

# Create windows projects for Watcom, VS 2022, and Codewarrior
MAKEPROJECTS = (
    {"platform": "windows",
     "ide": ("vs2003", "vs2022"),
     "type": "app",
     #"configuration": "Release_LTCG"},
    },
    {"platform": "windows",
     "ide": ("watcom", "codewarrior"),
     "type": "app",
     "configuration": "Release"
     }
)


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
    configuration.libraries_list.append("dinput8.lib")