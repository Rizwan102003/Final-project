from fastapi import FastAPI, File, UploadFile
import requests
from io import BytesIO
import tensorflow.keras as keras
app = FastAPI()
import numpy as np
from tensorflow.keras.preprocessing import image
import json
from tensorflow.keras.preprocessing.image import ImageDataGenerator

img_size = 128
batch_size = 32

train_datagen = ImageDataGenerator(
    rescale=1./255,
    validation_split=0.2,
    rotation_range=20,
    zoom_range=0.2,
    horizontal_flip=True
)
train_generator = train_datagen.flow_from_directory(
    './Dataset/crop_disease_detection/',
    target_size=(img_size, img_size),
    batch_size=batch_size,
    class_mode='categorical',
    subset='training'
)


MODEL_URL = "./weights/plant_disease_model.keras"

model = keras.models.load_model(MODEL_URL)

@app.get("/")
async def root():
    return {"message": "Welcome to the Image Prediction API"}

@app.post("/predict")
async def predict(file: UploadFile = File(...)):
    try:
        image_bytes = await file.read()
        print(f"Received file: {file.filename}, size: {len(image_bytes)} bytes")
        # img = image.load_img(img_path, target_size=(img_size, img_size))
        img_array = image.img_to_array(image.load_img(BytesIO(image_bytes), target_size=(128, 128)))
        img_array = np.expand_dims(img_array, axis=0) / 255.0

        prediction = model.predict(img_array)
        print(f"Prediction: {prediction}")
        predicted_class = np.argmax(prediction)
        class_label = list(train_generator.class_indices.keys())
        pred_class = class_label[predicted_class]       
        return {"prediction": pred_class}
    except Exception as e:
        return {"error": str(e)}

