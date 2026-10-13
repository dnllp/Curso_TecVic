import asyncio
import json
import websockets

CLIENTES = set()

async def handler(websocket):
    CLIENTES.add(websocket)
    client_id = id(websocket)
    print(f"[+] Cliente conectado: {client_id}")
    try:
        async for message in websocket:
            data = json.loads(message)
            print(f"[{client_id}] Dice: {data}")
            
            # Broadcast a todos los demás clientes conectados
            respuesta = json.dumps({"emisor": client_id, "payload": data})
            await asyncio.gather(*[c.send(respuesta) for c in CLIENTES])
            
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        CLIENTES.remove(websocket)
        print(f"[-] Cliente desconectado: {client_id}")

async def main():
    async with websockets.serve(handler, "localhost", 8080):
        print("Servidor WebSocket corriendo en ws://localhost:8080")
        await asyncio.Future()  # Mantener vivo

if __name__ == "__main__":
    asyncio.run(main())