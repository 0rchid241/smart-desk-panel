from fastapi import FastAPI

app = FastAPI(title="DeskMon Server")


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.get("/api/device/message")
def device_message() -> dict[str, str]:
    return {"message": "Hello from Lin"}
