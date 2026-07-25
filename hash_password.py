#!/usr/bin/env python3
# hash_password.py — config.json의 web_auth.password_hash 값 생성기
# 사용법: venv/bin/python3 hash_password.py [비밀번호]
#        인자를 생략하면 화면에 표시되지 않게 안전하게 입력받는다.

import argparse
import getpass
import sys

from werkzeug.security import generate_password_hash


def main():
    parser = argparse.ArgumentParser(
        description="config.json web_auth.password_hash에 넣을 해시 값을 생성합니다."
    )
    parser.add_argument("password", nargs="?", help="해시할 비밀번호 (생략 시 안전하게 입력받음)")
    args = parser.parse_args()

    password = args.password or getpass.getpass("비밀번호 입력: ")
    if not password:
        print("비밀번호가 비어 있습니다.", file=sys.stderr)
        sys.exit(1)

    print(generate_password_hash(password))


if __name__ == "__main__":
    main()
