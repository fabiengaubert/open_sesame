#!/usr/bin/env python3

import asyncio
import websockets
import logging
import traceback

PORT = 80
IP = "0.0.0.0"

# Configure logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger('WebSocket Server')

async def handle_client(websocket, path=None):
    try:
        logger.info(f"New connection from {websocket.remote_address}")

        async for message in websocket:
            try:
                logger.info(f"Received message: {message}")

                # Echo the message back
                await websocket.send(f"Server received: {message}")

            except Exception as process_err:
                logger.error(f"Error processing message: {process_err}")
                traceback.print_exc()

    except websockets.exceptions.ConnectionClosed as conn_closed:
        logger.warning(f"Connection closed: {conn_closed}")
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        traceback.print_exc()
    finally:
        logger.info("Client connection ended")

# Create and start the WebSocket server
async def main():
    try:
        server = await websockets.serve(handle_client, IP, PORT)
        logger.info("WebSocket server started on "+ IP + ":" + str(PORT))
        await server.wait_closed()

    except Exception as startup_err:
        logger.error(f"Server startup error: {startup_err}")
        traceback.print_exc()

# Run the server
if __name__ == "__main__":
    asyncio.run(main())