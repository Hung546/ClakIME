# run "just -l" to view all commands

default:
    @just --list

# build clak fcitx5 addon
build:
    cmake --build build

# build in release mode
build-release:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

# install addon for current user
install:
    cmake --build build --target install-user

# restart fcitx5 daemon
restart:
    fcitx5 -r -d

# build, install and restart fcitx5
dev: build install restart

# run all cargo tests
test-unit:
    cargo test --manifest-path engine/Cargo.toml

# run C++ state machine and regression test suites
test-cpp:
    cmake --build build --target clak_cpp_tests
    ./build/src/tests/clak_cpp_tests

# test typing speed and accuracy
test-speed delay="20":
    bash scripts/tests/test_speed.sh {{delay}}

# test chromium address bar typing and backspacing
test-chromium delay="15":
    bash scripts/tests/test_chromium.sh {{delay}}

# test autocomplete selection in address bar
test-autocomplete:
    bash scripts/tests/test_autocomplete_dd.sh

# test user typing scenario
test-scenario:
    bash scripts/tests/test_user_scenario.sh

# run all automated tests
test: test-unit test-cpp test-scenario

# run latency benchmark analysis and regression assertion
bench *args:
    ./bin/clak bench {{args}}

# run environment diagnostics
doctor:
    ./bin/clak doctor

# build clak settings gui binary
build-gui:
    cargo build --manifest-path ui/Cargo.toml --release

# run clak settings gui
gui:
    cargo run --manifest-path ui/Cargo.toml --release

# run all fmt, clippy, unit tests and build check before pushing
check:
    cargo fmt --manifest-path engine/Cargo.toml -- --check
    cargo clippy --manifest-path engine/Cargo.toml -- -D warnings
    cargo test --manifest-path engine/Cargo.toml
    cargo test --manifest-path ui/Cargo.toml
    cmake --build build --target clak_cpp_tests
    ./build/src/tests/clak_cpp_tests
    cmake --build build

# install git pre-push hook to run checks before pushing
install-hooks:
    @echo '#!/bin/sh' > .git/hooks/pre-push
    @echo 'echo "Running pre-push checks..."' >> .git/hooks/pre-push
    @echo 'just check || exit 1' >> .git/hooks/pre-push
    @chmod +x .git/hooks/pre-push
    @echo "pre-push hook installed successfully"

# tail debug log
log:
    tail -f /tmp/clak.log

# clear debug log
clean-log:
    rm -f /tmp/clak.log

# create cargo vendor archive for offline packaging
vendor:
    cargo vendor vendor/ --manifest-path engine/Cargo.toml
    tar -czf clak-vendor.tar.gz vendor/
    sha256sum clak-vendor.tar.gz > clak-vendor.tar.gz.sha256
    rm -rf vendor/

# update aur .srcinfo metadata
pkg-aur:
    cd packaging/aur && makepkg --printsrcinfo > .SRCINFO
    cd packaging/aur-bin && makepkg --printsrcinfo > .SRCINFO

# generate changelog with git-cliff
changelog:
    git-cliff --unreleased

# release a new version, bump files, commit, tag and push
tag version:
    #!/usr/bin/env bash
    set -euo pipefail
    raw="{{version}}"
    ver="${raw#v}"
    tag="v${ver}"
    branch=$(git branch --show-current)

    if ! git diff-index --quiet HEAD --; then
        echo "error: working tree is dirty, please commit or stash changes first"
        exit 1
    fi

    echo "Releasing ${tag} (version ${ver})..."

    # update versions across project files
    sed -i "s/project(clak VERSION [0-9.]\+/project(clak VERSION ${ver}/" CMakeLists.txt
    sed -i "s/^Version=[0-9.]\+/Version=${ver}/" data/clak-addon.conf.in
    sed -i "0,/^version = \"[0-9.]\+\"/s//version = \"${ver}\"/" engine/Cargo.toml
    sed -i "0,/^version = \"[0-9.]\+\"/s//version = \"${ver}\"/" ui/Cargo.toml
    sed -i "s/^pkgver=[0-9.]\+/pkgver=${ver}/" packaging/aur/PKGBUILD
    sed -i "s/^pkgrel=[0-9]\+/pkgrel=1/" packaging/aur/PKGBUILD
    sed -i "s/^pkgver=[0-9.]\+/pkgver=${ver}/" packaging/aur-bin/PKGBUILD
    sed -i "s/^pkgrel=[0-9]\+/pkgrel=1/" packaging/aur-bin/PKGBUILD

    cargo check --manifest-path engine/Cargo.toml --quiet
    cargo check --manifest-path ui/Cargo.toml --quiet
    (cd packaging/aur && makepkg --printsrcinfo > .SRCINFO)
    (cd packaging/aur-bin && makepkg --printsrcinfo > .SRCINFO)

    git add CMakeLists.txt data/clak-addon.conf.in engine/Cargo.toml engine/Cargo.lock ui/Cargo.toml ui/Cargo.lock packaging/aur/PKGBUILD packaging/aur/.SRCINFO packaging/aur-bin/PKGBUILD packaging/aur-bin/.SRCINFO
    git commit -m "chore: release ${tag}"
    git tag -a "${tag}" -m "release: ${tag}"

    git push origin "${branch}"
    git push origin "${tag}"
    echo "Successfully released and pushed ${tag}"

# test installer simulation flow
test-install mode="":
    bash scripts/install.sh --dry-run {{mode}}

# update clak to latest release
update args="":
    bash scripts/update.sh {{args}}

# test updater simulation flow
test-update args="":
    bash scripts/update.sh --dry-run {{args}}

# clean build artifacts
clean:
    rm -rf build engine/target vendor clak-vendor.tar.gz clak-vendor.tar.gz.sha256
