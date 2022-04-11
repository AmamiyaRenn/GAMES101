//
// Created by Göksu Güvendiren on 2019-05-14.
//

#include "Scene.hpp"

void Scene::buildBVH()
{
    printf(" - Generating BVH...\n\n");
    this->bvh = new BVHAccel(objects, 1, BVHAccel::SplitMethod::NAIVE);
}

Intersection Scene::intersect(const Ray &ray) const
{
    return this->bvh->Intersect(ray);
}

void Scene::sampleLight(Intersection &pos, float &pdf) const
{
    float emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k)
    {
        if (objects[k]->hasEmit())
        {
            emit_area_sum += objects[k]->getArea();
        }
    }
    float p = get_random_float() * emit_area_sum;
    emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k)
    {
        if (objects[k]->hasEmit())
        {
            emit_area_sum += objects[k]->getArea();
            if (p <= emit_area_sum)
            {
                objects[k]->Sample(pos, pdf);
                break;
            }
        }
    }
}

bool Scene::trace(
    const Ray &ray,
    const std::vector<Object *> &objects,
    float &tNear, uint32_t &index, Object **hitObject)
{
    *hitObject = nullptr;
    for (uint32_t k = 0; k < objects.size(); ++k)
    {
        float tNearK = kInfinity;
        uint32_t indexK;
        Vector2f uvK;
        if (objects[k]->intersect(ray, tNearK, indexK) && tNearK < tNear)
        {
            *hitObject = objects[k];
            tNear = tNearK;
            index = indexK;
        }
    }

    return (*hitObject != nullptr);
}

// TODO Implement Path Tracing Algorithm here
Vector3f Scene::castRay(const Ray &ray, int depth) const
{
    Intersection p_inter = intersect(ray); // 求一条光线与场景的交点
    if (!p_inter.happened)                 // 没交点
        return Vector3f(0, 0, 0);

    // 一、交点是光源：
    // 如果射线第一次打到光源，直接返回
    if (p_inter.m->hasEmission())
        if (depth == 0)
            return p_inter.m->getEmission(); // 获得光源颜色
        else
            return Vector3f(0, 0, 0);

    // 二、交点是物体：
    // 均匀采样光源物体，取光源上一点
    Intersection x_inter;
    float pdf_light = 0.0f;
    sampleLight(x_inter, pdf_light); // 获得对光源的采样，包括光源的位置和采样的pdf(在场景的所有光源上按面积 uniform sample 一个点，并计算该 sample 的概率密度)

    auto &N = p_inter.normal;                  // 物体表面法线
    auto &NN = x_inter.normal;                 // 灯光表面法线
    auto &objPos = p_inter.coords;             // 物体光线照射点坐标
    auto &lightPos = x_inter.coords;           // 采样的光源点坐标
    auto diff = lightPos - objPos;             // 物体反射点到光源采样点的向量
    auto lightDir = diff.normalized();         // 方向向量
    auto lightDis = dotProduct(diff, diff);    // 距离平方
    Ray light(objPos, lightDir);               // 从物体表面射出一条光线
    Intersection obj2light = intersect(light); // 求与光源的交

    Vector3f L_dir(0, 0, 0);
    // 如果该光线击中光源（及该光源可以直接照射到该点），计算直接光照值
    if (obj2light.happened && (obj2light.coords - lightPos).norm() < 1e-2)
    {
        auto f_r = p_inter.m->eval(ray.direction, lightDir, N); // 获得反射物体的材质
        //直接光照光 = 光源光 * brdf * 光线和物体角度衰减 * 光线和光源法线角度衰减 / 光线距离 / 该点的概率密度（1/该光源的面积）
        // f_r为BRDF，这里用的是简单漫反射，f_r= 漫 反 射 系 数 ( K d ) π 漫反射系数(Kd)\over \pi π漫反射系数(Kd)​
        // x_inter.emit是光照强度
        // dotProduct(lightDir, N)是物体非正向接收光照造成的能量衰减
        // dotProduct(-lightDir, NN) 是光源非正向光照传播造成的能量衰减
        // lightDis 是光照传播的距离，除以lightDis 及光照的距离衰减
        // pdf_light是小数，(通过代码分析知此处)为该光源的面积的倒数
        L_dir = x_inter.emit * f_r * dotProduct(lightDir, N) * dotProduct(-lightDir, NN) / lightDis / pdf_light;
    }

    Vector3f L_indir(0, 0, 0);
    // 俄罗斯轮盘赌，确定是否继续弹射光线
    if (get_random_float() > RussianRoulette) // 赌输
        return L_dir;

    auto nextRayDir = p_inter.m->sample(ray.direction, N).normalized(); // 从反射点随机采样一个新的弹射光线方向
    Ray nextRay(objPos, nextRayDir);                                    // 获得新的光线
    Intersection nextInter = intersect(nextRay);                        // 获得新的相交点
    if (nextInter.happened && !nextInter.m->hasEmission())              // 如果有与东西相交且不是与光源相交
    {
        float indir_pdf = p_inter.m->pdf(ray.direction, nextRayDir, N); // 间接光的pdf
        auto f_r = p_inter.m->eval(ray.direction, nextRayDir, N);       // 间接光的BRDF
        // 该点间接光= 弹射点反射光 * BRDF * 角度衰减 / pdf(认为该点四面八方都接收到了该方向的光强，为1/(2*pi)) / 俄罗斯轮盘赌值(强度矫正值)
        L_indir = castRay(nextRay, depth + 1) * f_r * dotProduct(nextRayDir, N) / indir_pdf / RussianRoulette;
    }
    return L_dir + L_indir; // 某点看到的光照为直接光照和间接光照的和
}