#!/bin/bash
# Setup Perl module environment for Git's MSYS2 Perl.
# Git for Windows bundles a stripped-down MSYS2 Perl that lacks core
# modules needed by OpenSSL's Configure script (IPC::Cmd, Pod::Usage,
# ExtUtils::MakeMaker, etc.).
#
# This script copies the missing pure-Perl modules from StrawberryPerl
# into a user-local directory and exports PERL5LIB so they are found.
#
# Usage: source scripts/setup_perl_env.sh

_strawberry_perl_lib() {
    for candidate in \
        "/d/SoftWare/strawberryPerl/perl/lib" \
        "/c/SoftWare/strawberryPerl/perl/lib" \
        "/c/Strawberry/perl/lib"; do
        if [ -d "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

_setup_perl_modules() {
    local local_lib="$HOME/perl5/lib/perl5"
    local strawberry_lib
    strawberry_lib=$(_strawberry_perl_lib) || {
        echo "ERROR: Cannot find StrawberryPerl installation" >&2
        return 1
    }

    # Already set up?
    if [ -d "$local_lib/ExtUtils" ] && [ -d "$local_lib/Pod" ]; then
        return 0
    fi

    echo "Setting up Perl modules for MSYS2 Perl from $strawberry_lib ..."
    rm -rf "$local_lib"
    mkdir -p "$local_lib"

    # Copy only pure-Perl module trees that Git's MSYS2 Perl lacks.
    # Do NOT copy compiled modules (POSIX, Config, Fcntl, etc.) —
    # those are version-specific and Git's Perl has its own.
    for pkg in IPC Params Locale Pod ExtUtils; do
        if [ -d "$strawberry_lib/$pkg" ]; then
            cp -r "$strawberry_lib/$pkg" "$local_lib/"
        fi
    done

    echo "  Copied $(find "$local_lib" -type f | wc -l) files"
}

_setup_perl_modules || return 1

# PERL5LIB must use Unix paths. Without MSYS2_ENV_CONV_EXCL, MSYS2
# converts /c/Users/... → C:/Users/... and Perl splits on ':' after
# the drive letter, turning one path into two: "C" and "/Users/...".
export MSYS2_ENV_CONV_EXCL="PERL5LIB${MSYS2_ENV_CONV_EXCL:+;$MSYS2_ENV_CONV_EXCL}"
export PERL5LIB="$HOME/perl5/lib/perl5${PERL5LIB:+:$PERL5LIB}"
