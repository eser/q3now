package main

import "errors"

var ErrImportInProgress = errors.New("import already in progress")
