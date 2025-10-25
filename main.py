from fastapi import FastAPI, File, UploadFile
import requests
from io import BytesIO

app = FastAPI()

MODEL_URL = "Samudraman i lost yo url"


@app.get("/")
async def root():
    return {"message": "Welcome to the Image Prediction API"}

@app.post("/predict")
async def predict(file: UploadFile = File(...)):
    image_bytes = await file.read()
    
    files = {"file": ("image.jpg", image_bytes, "image/jpeg")}
    response = requests.post(MODEL_URL, files=files)
    
    if response.status_code == 200:
        return response.json()
    else:
        return {"error": f"Model API returned {response.status_code}"}

