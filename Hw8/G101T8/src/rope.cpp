/***
 * @Description:
 * @Author: Amamiya
 * @Date: 2022-04-07 17:01:18
 * @TechChangeTheWorld
 * @WHUROBOCON_Alright_Reserved
 */
#include <iostream>
#include <vector>

#include "CGL/vector2D.h"

#include "mass.h"
#include "rope.h"
#include "spring.h"

namespace CGL
{

    Rope::Rope(Vector2D start, Vector2D end, int num_nodes, float node_mass, float k, vector<int> pinned_nodes)
    {
        Vector2D step = (end - start) / (num_nodes - 1);
        for (int i = 0; i < num_nodes; i++)
        {
            Mass *mass = new Mass(start + step * i, node_mass, false);
            mass->velocity = Vector2D(0, 0);
            if (i > 0)
            {
                Spring *spring = new Spring(masses.back(), mass, k);
                springs.push_back(spring);
            }
            masses.push_back(mass);
        }
        // Comment-in this part when you implement the constructor
        for (auto &i : pinned_nodes)
            masses[i]->pinned = true;
    }

    void Rope::simulateEuler(float delta_t, Vector2D gravity)
    {
        for (auto &s : springs)
        {
            Vector2D a2b = s->m2->position - s->m1->position;
            Vector2D f = -s->k * a2b.unit() * (a2b.norm() - s->rest_length);
            s->m1->forces -= f;
            s->m2->forces += f;
        }

        for (auto &m : masses)
        {
            if (!m->pinned)
            {
                // 重力
                m->forces += gravity * m->mass;
                // 阻尼
                float kd = 0.01f;
                Vector2D fd = -kd * m->velocity;
                m->forces += fd;
                // 计算加速度
                Vector2D a = m->forces / m->mass;
                // 下面的话在后面就是显式，前面就是隐式（因此隐式需要假设下一时刻a（加速度）已知）
                m->velocity += a * delta_t;
                m->position += m->velocity * delta_t;
            }
            // Reset all forces on each mass
            m->forces = Vector2D(0, 0);
        }
    }

    void Rope::simulateVerlet(float delta_t, Vector2D gravity)
    {
        for (auto &s : springs)
        {
            Vector2D a2b = s->m2->position - s->m1->position;
            Vector2D f = -s->k * a2b.unit() * (a2b.norm() - s->rest_length);
            s->m1->forces -= f;
            s->m2->forces += f;
        }

        for (auto &m : masses)
        {
            if (!m->pinned)
            {
                // 重力
                m->forces += gravity * m->mass;
                // 计算加速度
                Vector2D a = m->forces / m->mass;
                Vector2D temp_position = m->position;
                // 阻尼系数
                float dampingFactor = 0.0001f;
                // 计算带阻尼的Verlet算法
                m->position += (1 - dampingFactor) * (m->position - m->last_position) + a * delta_t * delta_t;
                m->last_position = temp_position;
            }
            m->forces = Vector2D(0, 0);
        }
    }
}
