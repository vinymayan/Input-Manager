#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace GestureMath {

    struct Point2D { float x, y; };

    // Calcula a distância entre dois pontos
    inline float Distance(Point2D p1, Point2D p2) {
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    // Calcula o comprimento total do caminho desenhado
    inline float PathLength(const std::vector<Point2D>& points) {
        float d = 0.0f;
        for (size_t i = 1; i < points.size(); i++) {
            d += Distance(points[i - 1], points[i]);
        }
        return d;
    }

    // PASSO 1: Força o desenho a ter exatamente N pontos (Ex: 64)
    inline std::vector<Point2D> Resample(const std::vector<Point2D>& points, int n) {
        float I = PathLength(points) / (n - 1);
        float D = 0.0f;
        std::vector<Point2D> newPoints;
        newPoints.push_back(points[0]);

        std::vector<Point2D> srcPts = points;

        for (size_t i = 1; i < srcPts.size(); i++) {
            float d = Distance(srcPts[i - 1], srcPts[i]);
            if ((D + d) >= I) {
                float qx = srcPts[i - 1].x + ((I - D) / d) * (srcPts[i].x - srcPts[i - 1].x);
                float qy = srcPts[i - 1].y + ((I - D) / d) * (srcPts[i].y - srcPts[i - 1].y);
                Point2D q = { qx, qy };
                newPoints.push_back(q);
                srcPts.insert(srcPts.begin() + i, q);
                D = 0.0f;
            }
            else {
                D += d;
            }
        }
        // Garante que o último ponto entra (lidando com arredondamentos de float)
        if (newPoints.size() == n - 1) newPoints.push_back(points.back());
        return newPoints;
    }

    // PASSO 2: Escala para uma caixa de 1x1
    inline std::vector<Point2D> ScaleToSquare(std::vector<Point2D> points) {
        float minX = 99999.0f, maxX = -99999.0f, minY = 99999.0f, maxY = -99999.0f;
        for (const auto& p : points) {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        }
        float width = maxX - minX;
        float height = maxY - minY;

        // Evita divisão por zero se for uma linha reta perfeita
        if (width == 0.0f) width = 0.001f;
        if (height == 0.0f) height = 0.001f;

        for (auto& p : points) {
            p.x = p.x * (1.0f / width);
            p.y = p.y * (1.0f / height);
        }
        return points;
    }

    // PASSO 3: Move o centro para (0,0)
    inline std::vector<Point2D> TranslateToOrigin(std::vector<Point2D> points) {
        float sumX = 0.0f, sumY = 0.0f;
        for (const auto& p : points) { sumX += p.x; sumY += p.y; }
        float centroidX = sumX / points.size();
        float centroidY = sumY / points.size();

        for (auto& p : points) {
            p.x -= centroidX;
            p.y -= centroidY;
        }
        return points;
    }

    // Função Mestra: Aplica todos os passos
    inline std::vector<Point2D> NormalizeGesture(const std::vector<Point2D>& rawPoints) {
        if (rawPoints.size() < 3) return rawPoints; // Inválido se for só um clique
        std::vector<Point2D> p = Resample(rawPoints, 64);
        p = ScaleToSquare(p);
        p = TranslateToOrigin(p);
        return p;
    }

    // Compara dois gestos normalizados e retorna uma pontuação de 0.0 (0%) a 1.0 (100%)
    inline float GetMatchScore(const std::vector<Point2D>& templateGesture, const std::vector<Point2D>& candidateGesture) {
        if (templateGesture.size() != candidateGesture.size()) return 0.0f;

        float distanceSum = 0.0f;
        for (size_t i = 0; i < templateGesture.size(); i++) {
            distanceSum += Distance(templateGesture[i], candidateGesture[i]);
        }

        float averageDistance = distanceSum / templateGesture.size();

        // A meia diagonal de um quadrado 1x1 é aproximadamente 0.707. 
        // Usamos isso como limite para o pior cenário.
        float score = 1.0f - (averageDistance / 0.707f);

        return std::max(0.0f, score); // Se a distância for terrível, retorna 0 em vez de negativo
    }
}