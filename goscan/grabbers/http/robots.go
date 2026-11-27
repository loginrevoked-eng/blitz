package http

import (
	"context"

	"goscan/scanner"
)

func Robots(ctx context.Context, target scanner.Target) (bool, error) {
	response, body, err := get(ctx, target, "/.env", 64*1024)
	if err != nil {
		return false, err
	}
	printBody(target, "/.env", response, body)
	return true, nil
}
