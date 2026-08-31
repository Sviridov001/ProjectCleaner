"""
Скрипт снятия префикса _null_ со слоёв ArchiCAD.
Запуск: python remove_null_prefix.py
Требуется: pip install archicad
АрхиКАД должен быть запущен.
"""
import json
import urllib.request

API_URL = "http://localhost:19991/jsonrpc"


def call_api(method, params=None):
    payload = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params:
        payload["params"] = params
    req = urllib.request.Request(
        API_URL,
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
    )
    resp = urllib.request.urlopen(req)
    return json.loads(resp.read())


def main():
    # Get all attributes
    result = call_api("AttributeManager.GetAttributeIds", {"attributeType": "Layer"})
    layer_ids = result["result"]["attributeIds"]

    renamed = 0
    skipped = 0

    for lid in layer_ids:
        info = call_api(
            "AttributeManager.GetAttributeById",
            {"attributeId": {"guid": lid["guid"]}},
        )
        name = info["result"]["attribute"]["name"]
        if name.startswith("_null_"):
            new_name = name[6:]
            call_api(
                "AttributeManager.RenameAttribute",
                {
                    "attributeId": {"guid": lid["guid"]},
                    "newName": new_name,
                },
            )
            print(f"  {name} -> {new_name}")
            renamed += 1
        else:
            skipped += 1

    print(f"\nГотово. Переименовано: {renamed}, пропущено: {skipped}")


if __name__ == "__main__":
    main()
