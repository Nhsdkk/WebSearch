import unittest

from fastapi import FastAPI
from fastapi.testclient import TestClient

from application.auth import BasicAuth, add_basic_auth


class BasicAuthTests(unittest.TestCase):
    def setUp(self) -> None:
        app = FastAPI(docs_url="/swagger")
        add_basic_auth(app, BasicAuth("username", "password"))

        @app.get("/protected")
        def protected() -> dict[str, bool]:
            return {"ok": True}

        self.client = TestClient(app)

    def test_missing_credentials_are_rejected(self) -> None:
        response = self.client.get("/protected")

        self.assertEqual(response.status_code, 401)
        self.assertEqual(response.headers["www-authenticate"], "Basic")

    def test_invalid_credentials_are_rejected(self) -> None:
        for username, password in (("wrong", "password"), ("username", "wrong")):
            with self.subTest(username=username, password=password):
                response = self.client.get("/protected", auth=(username, password))
                self.assertEqual(response.status_code, 401)
                self.assertEqual(response.headers["www-authenticate"], "Basic")

    def test_valid_credentials_are_accepted(self) -> None:
        response = self.client.get("/protected", auth=("username", "password"))

        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), {"ok": True})

    def test_empty_credentials_are_rejected(self) -> None:
        for username, password in (("", "password"), ("username", "")):
            with self.subTest(username=username):
                with self.assertRaises(ValueError):
                    BasicAuth(username, password)

    def test_docs_are_protected(self) -> None:
        for path in ("/swagger", "/openapi.json"):
            with self.subTest(path=path):
                response = self.client.get(path)
                self.assertEqual(response.status_code, 401)

    def test_swagger_schema_requires_basic_auth(self) -> None:
        response = self.client.get("/openapi.json", auth=("username", "password"))

        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["security"], [{"HTTPBasic": []}])
        self.assertEqual(
            response.json()["components"]["securitySchemes"]["HTTPBasic"],
            {"type": "http", "scheme": "basic"},
        )
