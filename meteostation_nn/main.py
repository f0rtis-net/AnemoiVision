# weather_forecast_conv1d.py
import os
import numpy as np
import pandas as pd
import tensorflow as tf
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import MinMaxScaler

# ============================
# 1. Параметры и пути
# ============================
DATA_PATH = "weather_data.csv"   # путь к CSV с погодными данными
MODEL_NAME = "weather_forecast_conv1d_esp32s3.tflite"
WINDOW_SIZE = 4   # сколько последних часов используем для прогноза

# ============================
# 2. Загрузка и подготовка данных с категоризацией WMO-кодов
# ============================
def load_forecast_data(path):
    import pandas as pd
    import numpy as np

    df = pd.read_csv(path)
    df = df.dropna()

    # ============================
    # Извлечение месяца и циклическое кодирование
    # ============================
    if 'time' in df.columns:
        df['month'] = pd.to_datetime(df['time']).dt.month
        df = df.drop(columns=['time'])
    elif 'datetime' in df.columns:
        df['month'] = pd.to_datetime(df['datetime']).dt.month
        df = df.drop(columns=['datetime'])
    else:
        raise ValueError("Нет колонки времени для извлечения месяца.")

    df['month_sin'] = np.sin(2 * np.pi * df['month'] / 12)
    df['month_cos'] = np.cos(2 * np.pi * df['month'] / 12)
    df = df.drop(columns=['month'])

    # ============================
    # Целевая переменная — код погоды
    # ============================
    target_col = 'weather_code (wmo code)'
    if target_col not in df.columns:
        raise ValueError(f"В файле нет столбца '{target_col}'. Найдены колонки: {df.columns.tolist()}")

    # ============================
    # Категоризация WMO-кодов
    # ============================
    wmo_groups = {
        'clear': [0, 1, 2, 3],         
        'fog': [45, 48],  
        'drizzle': [51, 53, 55, 56, 57],
        'rain': [61, 63, 65, 80, 81, 82],          
        'snow': [66, 67, 71, 73, 75, 77, 85, 86],  
        'other': []                     
    }

    code_to_group = {}
    for group_idx, (group_name, codes) in enumerate(wmo_groups.items()):
        for code in codes:
            code_to_group[code] = group_idx
    for code in df[target_col].unique():
        if code not in code_to_group:
            code_to_group[code] = len(wmo_groups) - 1

    df[target_col] = df[target_col].map(code_to_group)

    # ============================
    # Разделение X и y
    # ============================
    y = df[target_col].values
    X = df.drop(columns=[target_col]).values
    num_classes = 6

    print(f"Категории WMO-кодов: {wmo_groups}")
    print(f"Соответствие код->категория: {code_to_group}")
    print(f"Число классов: {num_classes}")

    return X, y, code_to_group, num_classes

# ============================
# 3. Создание последовательностей
# ============================
def make_sequence_data(X, y, window_size=6):
    X_seq, y_seq = [], []
    for i in range(len(X) - window_size):
        X_seq.append(X[i:i+window_size])
        y_seq.append(y[i+window_size])
    return np.array(X_seq), np.array(y_seq)

# ============================
# 4. Создание Conv1D модели
# ============================
def create_conv_forecast_model(input_shape, num_classes):
    model = tf.keras.Sequential([
        tf.keras.Input(shape=input_shape),
        tf.keras.layers.Conv1D(64, kernel_size=3, activation='relu', padding='same'),
        tf.keras.layers.Conv1D(128, kernel_size=3, activation='relu', padding='same'),
        tf.keras.layers.GlobalAveragePooling1D(),
        tf.keras.layers.Dense(128, activation='relu'),
        tf.keras.layers.Dropout(0.3),
        tf.keras.layers.Dense(num_classes, activation='softmax')
    ])

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    return model


def convert_to_tflite_esp32(model, X_sample, model_name):
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    def representative_dataset():
        for i in range(min(100, len(X_sample))):
            yield [X_sample[i:i+1].astype(np.float32)]

    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()
    with open(model_name, 'wb') as f:
        f.write(tflite_model)

    print(f"✅ Модель сохранена: {model_name} ({len(tflite_model)/1024:.1f} KB)")
    return tflite_model

# ============================
# 6. Основной сценарий
# ============================
def main_weather_forecast():
    print("Загрузка данных...")
    X, y, code_to_group, num_classes = load_forecast_data(DATA_PATH)

    print("Масштабирование...")
    scaler = MinMaxScaler()
    X_scaled = scaler.fit_transform(X)

    print("Формирование последовательностей...")
    X_seq, y_seq = make_sequence_data(X_scaled, y, WINDOW_SIZE)

    # Разделение на train/test
    X_train, X_test, y_train, y_test = train_test_split(X_seq, y_seq, test_size=0.2, random_state=42)

    print(f"Обучающих выборок: {X_train.shape[0]}, вход {X_train.shape[1:]}")

    print("Создание модели Conv1D + Dense...")
    model = create_conv_forecast_model(X_train.shape[1:], num_classes)

    # Колбэки
    early_stop = tf.keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True)

    print("Обучение модели...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_test, y_test),
        epochs=100,
        batch_size=64,
        callbacks=[early_stop],
        verbose=1
    )

    # Оценка
    print("\nОценка точности:")
    loss, acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"Loss: {loss:.4f}, Accuracy: {acc:.4f}")

    # Графики обучения
    plt.figure(figsize=(10,4))
    plt.plot(history.history['accuracy'], label='Train acc')
    plt.plot(history.history['val_accuracy'], label='Val acc')
    plt.title('Conv1D Weather Forecast Accuracy')
    plt.xlabel('Epoch')
    plt.ylabel('Accuracy')
    plt.legend()
    plt.grid(True)
    plt.show()

    # Сохранение TFLite-модели
    convert_to_tflite_esp32(model, X_train, MODEL_NAME)

    print("\n✅ Модель готова. Для перевода предсказания обратно в WMO-код используйте:")
    print(f"index_to_group = {{v: k for k, v in code_to_group.items()}}")

# ============================
# 7. Запуск
# ============================
if __name__ == "__main__":
    main_weather_forecast()
